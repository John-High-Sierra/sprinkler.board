import RPi.GPIO as GPIO
from flask import Flask, render_template, request, jsonify, redirect, url_for
import json
import time
import threading
import datetime
import os
import subprocess
import logging
import socket
import re

# --- Configuration ---
GPIO.setmode(GPIO.BCM)
SPRINKLER_PINS = [4, 22, 6, 26]
for pin in SPRINKLER_PINS:
    GPIO.setup(pin, GPIO.OUT)
    GPIO.output(pin, GPIO.LOW)

# --- Logging Setup ---
log_dir = os.path.join(os.path.expanduser("~"), "sprinkler_controller", "logs")
os.makedirs(log_dir, exist_ok=True)
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(os.path.join(log_dir, 'sprinkler_system.log')),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

# --- App Setup ---
app = Flask(__name__)
HOME_DIR = os.path.expanduser("~")
CONFIG_DIR = os.path.join(HOME_DIR, "sprinkler_controller")
SCHEDULE_FILE = os.path.join(CONFIG_DIR, "sprinkler_schedule.json")
NETWORK_CONFIG_FILE = os.path.join(CONFIG_DIR, "network_config.json")

# --- Global State Variables ---
current_status = {
    "is_running": False,
    "day_index": None,
    "active_sprinkler": None,
    "remaining_time": 0
}
schedule_enabled = True 
weekly_schedule = []
status_lock = threading.Lock()
schedule_lock = threading.Lock()
stop_event = threading.Event()
hotspot_mode = False

# --- Network Management Functions ---
def get_network_interfaces():
    """Get available network interfaces"""
    try:
        result = subprocess.run(['ip', 'link', 'show'], capture_output=True, text=True)
        interfaces = []
        for line in result.stdout.split('\n'):
            if ': ' in line and ('wlan' in line or 'eth' in line):
                interface = line.split(':')[1].strip().split('@')[0]
                interfaces.append(interface)
        return interfaces
    except:
        return ['wlan0', 'eth0']  # defaults

def scan_wifi_networks():
    """Scan for available WiFi networks"""
    try:
        subprocess.run(['sudo', 'ifconfig', 'wlan0', 'up'], capture_output=True)
        time.sleep(2)
        result = subprocess.run(['sudo', 'iwlist', 'wlan0', 'scan'], capture_output=True, text=True, timeout=15)
        
        networks = []
        current_network = {}
        
        for line in result.stdout.split('\n'):
            line = line.strip()
            if 'Cell ' in line and 'Address:' in line:
                if current_network:
                    networks.append(current_network)
                current_network = {}
            elif 'ESSID:' in line:
                essid = line.split('ESSID:')[1].strip().strip('"')
                if essid and essid != '<hidden>':
                    current_network['ssid'] = essid
            elif 'Quality=' in line:
                try:
                    quality = line.split('Quality=')[1].split(' ')[0]
                    current_network['quality'] = quality
                except:
                    pass
            elif 'Encryption key:' in line:
                current_network['encrypted'] = 'on' in line
        
        if current_network:
            networks.append(current_network)
        
        # Remove duplicates and sort by quality
        unique_networks = {}
        for net in networks:
            if 'ssid' in net:
                if net['ssid'] not in unique_networks or net.get('quality', '0/0') > unique_networks[net['ssid']].get('quality', '0/0'):
                    unique_networks[net['ssid']] = net
        
        return list(unique_networks.values())[:20]  # Return top 20 networks
        
    except Exception as e:
        logger.error(f"Error scanning WiFi: {e}")
        return []

def get_current_ip():
    """Get current IP address"""
    try:
        # Try to get IP from wlan0 first, then eth0
        for interface in ['wlan0', 'eth0']:
            result = subprocess.run(['ip', 'addr', 'show', interface], capture_output=True, text=True)
            for line in result.stdout.split('\n'):
                if 'inet ' in line and '127.0.0.1' not in line:
                    ip = line.strip().split(' ')[1].split('/')[0]
                    return ip, interface
        return None, None
    except:
        return None, None

def is_connected_to_internet():
    """Check if device has internet connection"""
    try:
        socket.create_connection(("8.8.8.8", 53), timeout=3)
        return True
    except OSError:
        return False

def configure_wifi(ssid, password):
    """Configure WiFi connection"""
    try:
        # Create wpa_supplicant configuration
        wpa_config = f"""
country=US
ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
update_config=1

network={{
    ssid="{ssid}"
    psk="{password}"
    key_mgmt=WPA-PSK
}}
"""
        
        # Write configuration
        with open('/tmp/wpa_supplicant.conf', 'w') as f:
            f.write(wpa_config)
        
        # Copy to system location
        subprocess.run(['sudo', 'cp', '/tmp/wpa_supplicant.conf', '/etc/wpa_supplicant/wpa_supplicant.conf'], check=True)
        
        # Restart networking
        subprocess.run(['sudo', 'systemctl', 'restart', 'dhcpcd'], capture_output=True)
        subprocess.run(['sudo', 'wpa_cli', '-i', 'wlan0', 'reconfigure'], capture_output=True)
        
        logger.info(f"WiFi configured for network: {ssid}")
        return True
        
    except Exception as e:
        logger.error(f"Error configuring WiFi: {e}")
        return False

def start_hotspot():
    """Start WiFi hotspot mode"""
    global hotspot_mode
    try:
        logger.info("Starting WiFi hotspot...")
        
        # Configure hostapd
        hostapd_conf = """
interface=wlan0
driver=nl80211
ssid=SprinklerController
hw_mode=g
channel=7
wmm_enabled=0
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=sprinkler123
wpa_key_mgmt=WPA-PSK
wpa_pairwise=TKIP
rsn_pairwise=CCMP
"""
        
        with open('/tmp/hostapd.conf', 'w') as f:
            f.write(hostapd_conf)
        subprocess.run(['sudo', 'cp', '/tmp/hostapd.conf', '/etc/hostapd/hostapd.conf'], check=True)
        
        # Configure dnsmasq
        dnsmasq_conf = """
interface=wlan0
dhcp-range=192.168.4.2,192.168.4.20,255.255.255.0,24h
"""
        
        with open('/tmp/dnsmasq.conf', 'w') as f:
            f.write(dnsmasq_conf)
        subprocess.run(['sudo', 'cp', '/tmp/dnsmasq.conf', '/etc/dnsmasq.conf'], check=True)
        
        # Configure network interface
        subprocess.run(['sudo', 'ifconfig', 'wlan0', '192.168.4.1'], check=True)
        
        # Start services
        subprocess.run(['sudo', 'systemctl', 'start', 'hostapd'], check=True)
        subprocess.run(['sudo', 'systemctl', 'start', 'dnsmasq'], check=True)
        
        hotspot_mode = True
        logger.info("WiFi hotspot started successfully")
        return True
        
    except Exception as e:
        logger.error(f"Error starting hotspot: {e}")
        return False

def stop_hotspot():
    """Stop WiFi hotspot mode"""
    global hotspot_mode
    try:
        subprocess.run(['sudo', 'systemctl', 'stop', 'hostapd'], capture_output=True)
        subprocess.run(['sudo', 'systemctl', 'stop', 'dnsmasq'], capture_output=True)
        hotspot_mode = False
        logger.info("WiFi hotspot stopped")
        return True
    except Exception as e:
        logger.error(f"Error stopping hotspot: {e}")
        return False

# --- Data Handling Functions ---
def load_schedule():
    global schedule_enabled
    try:
        with open(SCHEDULE_FILE, 'r') as f:
            data = json.load(f)
            if isinstance(data, list): 
                schedule = data
                schedule_enabled = True 
            elif isinstance(data, dict): 
                schedule = data.get("schedule", [])
                schedule_enabled = data.get("enabled", True)
            else: 
                raise ValueError("Unknown schedule format")
            logger.info("Schedule loaded successfully")
            return schedule
    except (FileNotFoundError, ValueError) as e:
        logger.warning(f"Could not load schedule file: {e}. Using default schedule.")
        return [
            {"is_active": False, "hour": 7, "minute": 0, "durations": [10, 10, 10, 10]}
            for _ in range(7)
        ]

def save_schedule(schedule_data):
    data_to_save = {
        "enabled": schedule_enabled,
        "schedule": schedule_data
    }
    try:
        with schedule_lock:
            with open(SCHEDULE_FILE, 'w') as f:
                json.dump(data_to_save, f, indent=4)
        logger.info("Schedule saved successfully")
    except Exception as e:
        logger.error(f"Failed to save schedule: {e}")

# --- Core Sprinkler Logic (same as before) ---
def run_sequence(day_index):
    global current_status, stop_event
    stop_event.clear()
    
    if not 0 <= day_index < 7:
        logger.error(f"Invalid day index: {day_index}")
        return
    
    day_schedule = weekly_schedule[day_index]
    day_name = ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday', 'Sunday'][day_index]
    logger.info(f"Starting sprinkler sequence for {day_name}")
    
    try:
        for i, duration_min in enumerate(day_schedule['durations']):
            if stop_event.is_set():
                logger.info("Stop event received, halting sequence.")
                break 
            
            if duration_min > 0:
                logger.info(f"Starting sprinkler {i+1} for {duration_min} minutes")
                total_seconds = duration_min * 60
                
                GPIO.output(SPRINKLER_PINS[i], GPIO.HIGH)
                remaining = total_seconds
                
                while remaining > 0:
                    if stop_event.is_set():
                        logger.info(f"Stop event received during sprinkler {i+1} run.")
                        break
                    
                    with status_lock:
                        current_status = {
                            "is_running": True,
                            "day_index": day_index,
                            "active_sprinkler": i,
                            "remaining_time": remaining
                        }
                    time.sleep(1)
                    remaining -= 1
                
                GPIO.output(SPRINKLER_PINS[i], GPIO.LOW)
                logger.info(f"Sprinkler {i+1} completed")
                
                if stop_event.is_set():
                    break
                    
            else:
                logger.info(f"Sprinkler {i+1} skipped (duration = 0)")
                
    except Exception as e:
        logger.error(f"Error during sprinkler sequence: {e}")
    finally:
        logger.info("Sequence finished or stopped. Cleaning up...")
        for pin in SPRINKLER_PINS:
            GPIO.output(pin, GPIO.LOW)
        
        with status_lock:
            current_status = {
                "is_running": False,
                "day_index": None,
                "active_sprinkler": None,
                "remaining_time": 0
            }

def schedule_checker():
    global schedule_enabled
    last_run_day = -1
    last_check_minute = -1
    
    logger.info("Schedule checker thread started")
    
    while True:
        try:
            with schedule_lock:
                local_schedule_enabled = schedule_enabled
                local_weekly_schedule = list(weekly_schedule)

            if local_schedule_enabled:
                now = datetime.datetime.now()
                current_day = now.weekday()
                current_hour = now.hour
                current_minute = now.minute

                if current_minute != last_check_minute:
                    last_check_minute = current_minute
                    
                    if current_hour == 0 and current_minute == 0:
                        last_run_day = -1
                        logger.info("Daily run flag reset at midnight")

                    if current_day != last_run_day and current_day < len(local_weekly_schedule):
                        day_schedule = local_weekly_schedule[current_day]
                        if (day_schedule['is_active'] and
                            day_schedule['hour'] == current_hour and
                            day_schedule['minute'] == current_minute):
                            
                            with status_lock:
                                is_running = current_status["is_running"]
                            
                            if not is_running:
                                day_name = ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday', 'Sunday'][current_day]
                                logger.info(f"Scheduled run starting for {day_name} at {current_hour:02d}:{current_minute:02d}")
                                last_run_day = current_day
                                threading.Thread(target=run_sequence, args=(current_day,), daemon=True).start()
                            else:
                                logger.warning("Scheduled run skipped - another sequence is already running")
                                
        except Exception as e:
            logger.error(f"Error in schedule checker: {e}")
            
        time.sleep(30)

# --- Time Management Functions (same as before) ---
def get_current_time():
    try:
        now = datetime.datetime.now()
        return now.strftime("%Y-%m-%d %H:%M:%S")
    except Exception as e:
        logger.error(f"Error getting current time: {e}")
        return "Error getting time"

def set_system_time(year, month, day, hour, minute):
    try:
        datetime.datetime(year, month, day, hour, minute, 0)
        
        time_string = f"{year}-{month:02d}-{day:02d} {hour:02d}:{minute:02d}:00"
        
        try:
            subprocess.run(['sudo', 'timedatectl', 'set-ntp', 'false'], 
                         capture_output=True, text=True, timeout=10)
            logger.info("NTP disabled for manual time setting")
        except Exception as e:
            logger.warning(f"Could not disable NTP: {e}")
        
        command = ['sudo', 'timedatectl', 'set-time', time_string]
        logger.info(f"Executing command: {' '.join(command)}")
        
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        
        if result.returncode == 0:
            logger.info(f"Time set successfully to {time_string}")
            return True, "Time updated successfully"
        else:
            error_msg = result.stderr.strip() if result.stderr else "Unknown error"
            logger.warning(f"timedatectl failed: {error_msg}")
            
            date_string = f"{month:02d}{day:02d}{hour:02d}{minute:02d}{year}"
            date_command = ['sudo', 'date', date_string]
            logger.info(f"Trying fallback date command: {' '.join(date_command)}")
            
            result2 = subprocess.run(date_command, capture_output=True, text=True, timeout=10)
            
            if result2.returncode == 0:
                logger.info(f"Time set successfully using date command")
                return True, "Time updated successfully (using date command)"
            else:
                error_msg2 = result2.stderr.strip() if result2.stderr else "Unknown error"
                logger.error(f"Both time setting methods failed. timedatectl: {error_msg}, date: {error_msg2}")
                return False, f"Failed to set time. timedatectl error: {error_msg}, date error: {error_msg2}"
                
    except ValueError as e:
        logger.error(f"Invalid date/time values: {e}")
        return False, f"Invalid date/time values: {e}"
    except subprocess.TimeoutExpired:
        logger.error("Time setting command timed out")
        return False, "Time setting command timed out"
    except Exception as e:
        logger.error(f"Unexpected error setting time: {e}")
        return False, f"Unexpected error: {e}"

# --- Flask Routes ---
@app.route('/')
def index():
    return render_template('index.html')

@app.route('/network')
def network_setup():
    return render_template('network.html')

# API Routes (include all previous routes plus new network routes)
@app.route('/api/schedule', methods=['GET', 'POST'])
def handle_schedule():
    global weekly_schedule
    if request.method == 'POST':
        try:
            new_schedule = request.json
            if not isinstance(new_schedule, list) or len(new_schedule) != 7:
                return jsonify({"error": "Invalid schedule format"}), 400
            
            with schedule_lock:
                weekly_schedule = new_schedule
            save_schedule(weekly_schedule)
            logger.info("Schedule updated via API")
            return jsonify({"message": "Schedule updated"})
        except Exception as e:
            logger.error(f"Error updating schedule: {e}")
            return jsonify({"error": str(e)}), 500
    else:
        with schedule_lock:
            return jsonify({"schedule": weekly_schedule, "enabled": schedule_enabled})

@app.route('/api/status')
def get_status():
    with status_lock:
        return jsonify(current_status)

@app.route('/api/run_day/<int:day_index>', methods=['POST'])
def run_day_now(day_index):
    if not 0 <= day_index < 7:
        return jsonify({"message": "Invalid day index"}), 400
    
    with status_lock:
        if current_status["is_running"]:
            return jsonify({"message": "A sequence is already running"}), 409
    
    logger.info(f"Manual run requested for day index {day_index}")
    threading.Thread(target=run_sequence, args=(day_index,), daemon=True).start()
    return jsonify({"message": "Sequence started"})

@app.route('/api/toggle_schedule', methods=['POST'])
def toggle_schedule():
    global schedule_enabled
    with schedule_lock:
        schedule_enabled = not schedule_enabled
    save_schedule(weekly_schedule)
    logger.info(f"Schedule {'enabled' if schedule_enabled else 'disabled'}")
    return jsonify({"enabled": schedule_enabled})

@app.route('/api/stop_sequence', methods=['POST'])
def stop_sequence_endpoint():
    global stop_event
    with status_lock:
        if not current_status["is_running"]:
            return jsonify({"message": "No sequence is currently running"}), 400
    
    logger.info("Stop request received from API")
    stop_event.set()
    return jsonify({"message": "Stop signal sent successfully"})

@app.route('/api/get_time')
def get_time():
    try:
        current_time = get_current_time()
        return jsonify({"time": current_time})
    except Exception as e:
        logger.error(f"Error getting time: {e}")
        return jsonify({"error": str(e)}), 500

@app.route('/api/set_time', methods=['POST'])
def set_time():
    try:
        data = request.json
        if not data:
            return jsonify({"error": "No data provided"}), 400
        
        required_fields = ['year', 'month', 'day', 'hour', 'minute']
        for field in required_fields:
            if field not in data:
                return jsonify({"error": f"Missing required field: {field}"}), 400
        
        year = int(data['year'])
        month = int(data['month'])
        day = int(data['day'])
        hour = int(data['hour'])
        minute = int(data['minute'])
        
        if not (1900 <= year <= 2100):
            return jsonify({"error": "Year must be between 1900 and 2100"}), 400
        if not (1 <= month <= 12):
            return jsonify({"error": "Month must be between 1 and 12"}), 400
        if not (1 <= day <= 31):
            return jsonify({"error": "Day must be between 1 and 31"}), 400
        if not (0 <= hour <= 23):
            return jsonify({"error": "Hour must be between 0 and 23"}), 400
        if not (0 <= minute <= 59):
            return jsonify({"error": "Minute must be between 0 and 59"}), 400
        
        success, message = set_system_time(year, month, day, hour, minute)
        
        if success:
            return jsonify({"message": message})
        else:
            return jsonify({"error": message}), 500
            
    except ValueError as e:
        logger.error(f"Invalid input for time setting: {e}")
        return jsonify({"error": f"Invalid input: {e}"}), 400
    except Exception as e:
        logger.error(f"Unexpected error in set_time endpoint: {e}")
        return jsonify({"error": f"Unexpected server error: {e}"}), 500

# --- New Network API Routes ---
@app.route('/api/network_status')
def network_status():
    try:
        ip, interface = get_current_ip()
        internet = is_connected_to_internet()
        
        return jsonify({
            "ip_address": ip,
            "interface": interface,
            "connected": ip is not None,
            "internet": internet,
            "hotspot_mode": hotspot_mode
        })
    except Exception as e:
        logger.error(f"Error getting network status: {e}")
        return jsonify({"error": str(e)}), 500

@app.route('/api/wifi_scan')
def wifi_scan():
    try:
        networks = scan_wifi_networks()
        return jsonify({"networks": networks})
    except Exception as e:
        logger.error(f"Error scanning WiFi: {e}")
        return jsonify({"error": str(e)}), 500

@app.route('/api/connect_wifi', methods=['POST'])
def connect_wifi():
    try:
        data = request.json
        ssid = data.get('ssid')
        password = data.get('password')
        
        if not ssid:
            return jsonify({"error": "SSID is required"}), 400
        
        if configure_wifi(ssid, password):
            # Stop hotspot if it's running
            if hotspot_mode:
                stop_hotspot()
            
            return jsonify({"message": "WiFi configuration updated. Please wait for connection..."})
        else:
            return jsonify({"error": "Failed to configure WiFi"}), 500
            
    except Exception as e:
        logger.error(f"Error connecting to WiFi: {e}")
        return jsonify({"error": str(e)}), 500

@app.route('/api/start_hotspot', methods=['POST'])
def start_hotspot_endpoint():
    try:
        if start_hotspot():
            return jsonify({"message": "Hotspot started successfully"})
        else:
            return jsonify({"error": "Failed to start hotspot"}), 500
    except Exception as e:
        logger.error(f"Error starting hotspot: {e}")
        return jsonify({"error": str(e)}), 500

@app.route('/api/system_info')
def get_system_info():
    try:
        ip, interface = get_current_ip()
        info = {
            "current_time": get_current_time(),
            "schedule_enabled": schedule_enabled,
            "gpio_pins": SPRINKLER_PINS,
            "schedule_file": SCHEDULE_FILE,
            "ip_address": ip,
            "network_interface": interface,
            "hotspot_mode": hotspot_mode,
            "internet_connected": is_connected_to_internet()
        }
        
        try:
            result = subprocess.run(['whoami'], capture_output=True, text=True, timeout=5)
            info["user"] = result.stdout.strip()
        except:
            info["user"] = "unknown"
        
        try:
            result = subprocess.run(['which', 'timedatectl'], capture_output=True, text=True, timeout=5)
            info["timedatectl_available"] = result.returncode == 0
        except:
            info["timedatectl_available"] = False
        
        return jsonify(info)
    except Exception as e:
        logger.error(f"Error getting system info: {e}")
        return jsonify({"error": str(e)}), 500

# --- Initialize system on startup ---
def initialize_system():
    """Initialize the system on startup"""
    logger.info("Initializing system...")
    
    # Check network connectivity
    ip, interface = get_current_ip()
    if not ip:
        logger.info("No network connection found, starting in hotspot mode...")
        start_hotspot()
    else:
        logger.info(f"Network connection found: {ip} on {interface}")

# --- Main Execution ---
if __name__ == '__main__':
    try:
        logger.info("Starting Standalone Sprinkler Control System")
        
        # Initialize system
        initialize_system()
        
        # Load initial schedule
        weekly_schedule = load_schedule()
        save_schedule(weekly_schedule)
        
        # Start background scheduler thread
        scheduler_thread = threading.Thread(target=schedule_checker, daemon=True)
        scheduler_thread.start()
        logger.info("Background scheduler started")
        
        # Start Flask app
        logger.info("Starting Flask web server on port 5000")
        app.run(host='0.0.0.0', port=5000, debug=False)
        
    except KeyboardInterrupt:
        logger.info("Shutdown requested by user")
    except Exception as e:
        logger.error(f"Fatal error: {e}")
    finally:
        logger.info("Cleaning up GPIO...")
        stop_event.set()
        GPIO.cleanup()
        logger.info("Sprinkler Control System shutdown complete")
