#!/bin/bash
# Fix WiFi service conflicts preventing hotspot

echo "=== Fix WiFi Service Conflicts ==="
echo ""

if [ "$EUID" -ne 0 ]; then
    echo "This script must be run with sudo"
    echo "Usage: sudo bash wifi_conflict_fix.sh"
    exit 1
fi

echo "The issue: hostapd starts but gets terminated by conflicting services"
echo ""

echo "Step 1: Identify conflicting services..."
echo "Checking what's managing wlan0:"

# Check what services might be controlling WiFi
echo "NetworkManager status:"
systemctl is-active NetworkManager 2>/dev/null || echo "Not running"

echo "wpa_supplicant status:"
systemctl is-active wpa_supplicant 2>/dev/null || echo "Not running"

echo "dhcpcd status:"
systemctl is-active dhcpcd 2>/dev/null || echo "Not running"

echo ""
echo "Step 2: Stop conflicting services temporarily..."

# Stop services that might interfere with hostapd
echo "Stopping wpa_supplicant..."
systemctl stop wpa_supplicant 2>/dev/null || true
systemctl stop wpa_supplicant@wlan0 2>/dev/null || true

echo "Stopping dhcpcd..."
systemctl stop dhcpcd 2>/dev/null || true

echo "Stopping NetworkManager if running..."
systemctl stop NetworkManager 2>/dev/null || true

# Wait for services to fully stop
sleep 3

echo ""
echo "Step 3: Kill any remaining WiFi processes..."
pkill wpa_supplicant 2>/dev/null || true
pkill dhcpcd 2>/dev/null || true
sleep 2

echo ""
echo "Step 4: Reset WiFi interface completely..."
# Completely reset the interface
ip link set wlan0 down
sleep 1
ip addr flush dev wlan0
sleep 1
ip link set wlan0 up
sleep 2

# Set the static IP for hotspot
ip addr add 192.168.4.1/24 dev wlan0
sleep 1

echo "Interface status:"
ip addr show wlan0

echo ""
echo "Step 5: Start dnsmasq for DHCP..."
systemctl restart dnsmasq
sleep 2

echo "Dnsmasq status:"
systemctl is-active dnsmasq && echo "Running" || echo "Failed"

echo ""
echo "Step 6: Start hostapd and keep it running..."

# Create a simple script to keep hostapd running
cat > /tmp/start_hotspot.sh << 'EOF'
#!/bin/bash
# Kill any existing hostapd
pkill hostapd 2>/dev/null || true
sleep 1

# Start hostapd in foreground
echo "Starting hostapd..."
hostapd /etc/hostapd/hostapd.conf
EOF

chmod +x /tmp/start_hotspot.sh

echo "Starting hostapd in background..."
nohup /tmp/start_hotspot.sh > /tmp/hostapd.log 2>&1 &
HOSTAPD_PID=$!

# Wait a moment for it to start
sleep 5

echo ""
echo "Step 7: Check if hotspot is working..."
echo "Hostapd process:"
if ps aux | grep -v grep | grep hostapd; then
    echo "✓ Hostapd is running"
else
    echo "✗ Hostapd is not running"
    echo "Hostapd log:"
    cat /tmp/hostapd.log 2>/dev/null || echo "No log available"
fi

echo ""
echo "Network scan for SprinklerController:"
iwlist wlan0 scan 2>/dev/null | grep -A5 -B5 "SprinklerController" || echo "Not found in scan"

echo ""
echo "Interface status:"
ip addr show wlan0 | grep inet

echo ""
echo "Step 8: Test web server access..."
echo "Testing if web server responds on hotspot IP..."
if curl -s --connect-timeout 5 http://192.168.4.1:5000 >/dev/null; then
    echo "✓ Web server accessible on hotspot IP"
else
    echo "✗ Web server not accessible - checking sprinkler service..."
    systemctl status sprinkler-controller.service --no-pager -l | tail -10
fi

echo ""
echo "Step 9: Create permanent solution..."

# Create a service to manage hotspot mode
cat > /etc/systemd/system/sprinkler-hotspot.service << 'EOF'
[Unit]
Description=Sprinkler Controller Hotspot
After=network.target
Conflicts=wpa_supplicant.service dhcpcd.service NetworkManager.service

[Service]
Type=simple
ExecStartPre=/bin/bash -c 'systemctl stop wpa_supplicant dhcpcd NetworkManager 2>/dev/null || true'
ExecStartPre=/bin/sleep 2
ExecStartPre=/sbin/ip link set wlan0 down
ExecStartPre=/sbin/ip addr flush dev wlan0
ExecStartPre=/sbin/ip addr add 192.168.4.1/24 dev wlan0
ExecStartPre=/sbin/ip link set wlan0 up
ExecStart=/usr/sbin/hostapd /etc/hostapd/hostapd.conf
ExecStop=/bin/killall hostapd
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

echo "✓ Created dedicated hotspot service"

echo ""
echo "Step 10: Manual start test..."
echo "Stopping current hostapd and testing service..."

# Stop our manual hostapd
kill $HOSTAPD_PID 2>/dev/null || true
pkill hostapd 2>/dev/null || true
sleep 2

# Test the new service
systemctl daemon-reload
systemctl start sprinkler-hotspot.service
sleep 5

echo "Hotspot service status:"
systemctl status sprinkler-hotspot.service --no-pager -l

echo ""
echo "Final network scan:"
iwlist wlan0 scan 2>/dev/null | grep -A3 -B3 "SprinklerController" || echo "Still not found"

echo ""
echo "=== Fix Complete ==="
echo ""
echo "Two services are now available:"
echo "1. sprinkler-hotspot.service - Dedicated hotspot service"
echo "2. sprinkler-controller.service - Main sprinkler app"
echo ""
echo "To use the dedicated hotspot:"
echo "- Start: sudo systemctl start sprinkler-hotspot.service"
echo "- Enable: sudo systemctl enable sprinkler-hotspot.service"
echo "- Check: sudo systemctl status sprinkler-hotspot.service"
echo ""
echo "If you see 'SprinklerController' network:"
echo "- Connect with password: sprinkler123"
echo "- Go to: http://192.168.4.1:5000"
echo ""
echo "Logs to check:"
echo "- Hotspot: sudo journalctl -u sprinkler-hotspot.service -f"
echo "- App: sudo journalctl -u sprinkler-controller.service -f"