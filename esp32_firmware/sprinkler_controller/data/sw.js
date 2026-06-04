self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', e => e.waitUntil(clients.claim()));
self.addEventListener('fetch', e => {
  if (e.request.mode === 'navigate') {
    e.respondWith(
      fetch(e.request).catch(() => new Response(
        '<!DOCTYPE html><html><body style="font-family:sans-serif;background:#0f1923;color:#e2e8f0;display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0"><div style="text-align:center;padding:24px"><div style="font-size:3rem;margin-bottom:16px">💧</div><h2 style="color:#0d9488;margin-bottom:8px">Board Unreachable</h2><p style="color:#607080;margin-bottom:20px">Connect via Tailscale or your home network.</p><button onclick="location.reload()" style="padding:12px 24px;background:#0d9488;color:#fff;border:none;border-radius:8px;cursor:pointer;font-size:1rem">Retry</button></div></body></html>',
        { headers: { 'Content-Type': 'text/html' } }
      ))
    );
  }
});
