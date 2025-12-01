<?php
// Simple UI: devices | sending | receiving panels
require_once __DIR__ . '/chat_lib.php';
?>
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>Device Chat Control</title>
    <style>
        body { font-family: Arial, sans-serif; background: #f4f4f4; padding: 20px; }
        .container { display: flex; gap: 16px; }
        .panel { background: #fff; padding: 12px; border-radius: 6px; box-shadow: 0 1px 4px rgba(0,0,0,0.1); flex: 1; }
        h3 { margin: 6px 0 12px 0; }
        select, input[type=text], button { width: 100%; padding: 8px; margin: 6px 0; box-sizing: border-box; }
        #log { height: 300px; overflow:auto; background:#111; color:#0f0; padding:8px; font-family: monospace; white-space: pre-wrap }
        .device-item { padding:6px; border-bottom:1px solid #eee }
    </style>
</head>
<body>
    <h2>Train Device Chat</h2>
    <div class="container">
        <div class="panel" style="max-width:260px;">
            <h3>Devices</h3>
            <div id="devices"></div>
            <button id="refreshDevices">Refresh</button>
            <button id="resetBtn" style="margin-top:8px;background:#c33;color:#fff;">Reset Store</button>
        </div>

        <div class="panel">
            <h3>Sending</h3>
            <label for="deviceSelect">Device</label>
            <select id="deviceSelect"></select>
            <label for="messageInput">Message</label>
            <input type="text" id="messageInput" placeholder="e.g. train go to station 2">
            <button id="sendBtn">Send</button>

            <hr>
            <label for="orderInput">Order (k=v;k2=v2)</label>
            <input type="text" id="orderInput" placeholder="e.g. action=move;from=1;to=2;switch1=on">
            <button id="sendOrderBtn">Send Order</button>
        </div>

        <div class="panel">
            <h3>Receiving (log)</h3>
            <div id="log">(no messages yet)</div>
        </div>
    </div>

    <script>
        const api = 'data.php';
        const devicesDiv = document.getElementById('devices');
        const deviceSelect = document.getElementById('deviceSelect');
        const sendBtn = document.getElementById('sendBtn');
        const messageInput = document.getElementById('messageInput');
        const logDiv = document.getElementById('log');

        async function listDevices(){
            try{
                const res = await fetch(api + '?action=list_devices');
                const data = await res.json();
                devicesDiv.innerHTML = '';
                deviceSelect.innerHTML = '';
                for(const id in data){
                    const d = data[id];
                    const el = document.createElement('div');
                    el.className = 'device-item';
                    const last = d.lastSeen ? new Date(d.lastSeen*1000).toLocaleTimeString() : 'never';
                    el.textContent = id + ' — lastSeen: ' + last;
                    devicesDiv.appendChild(el);
                    const opt = document.createElement('option'); opt.value = id; opt.textContent = id; deviceSelect.appendChild(opt);
                }
                if(Object.keys(data).length===0){ devicesDiv.textContent = 'No devices connected'; }
            }catch(e){ devicesDiv.textContent = 'Error loading devices'; }
        }

        async function sendMessage(){
            const device = deviceSelect.value;
            const msg = messageInput.value.trim();
            if(!device || !msg) return alert('Select device and enter message');
            const u = api + '?action=send&device=' + encodeURIComponent(device) + '&msg=' + encodeURIComponent(msg);
            await fetch(u);
            messageInput.value = '';
            await refreshLog();
        }

        async function refreshLog(){
            const device = deviceSelect.value;
            if(!device) { logDiv.textContent = '(select device)'; return; }
            const res = await fetch(api + '?action=get_log&device=' + encodeURIComponent(device));
            const arr = await res.json();
            if(!Array.isArray(arr)) { logDiv.textContent = 'No log'; return; }
            let text = '';
            for(const e of arr){
                const t = new Date(e.time*1000).toLocaleTimeString();
                text += '['+t+'] ' + e.from.toUpperCase() + ': ' + e.msg + '\n';
            }
            logDiv.textContent = text || '(empty)';
            logDiv.scrollTop = logDiv.scrollHeight;
        }

        document.getElementById('refreshDevices').addEventListener('click', listDevices);
        sendBtn.addEventListener('click', sendMessage);
        deviceSelect.addEventListener('change', refreshLog);
        document.getElementById('resetBtn').addEventListener('click', async function(){
            if(!confirm('Delete ALL devices and messages? This cannot be undone.')) return;
            try{
                const res = await fetch(api + '?action=reset');
                const text = await res.text();
                if(res.ok) {
                    alert('Store reset');
                    listDevices();
                    refreshLog();
                } else {
                    alert('Reset failed: ' + text);
                }
            }catch(e){ alert('Reset error'); }
        });

        document.getElementById('sendOrderBtn').addEventListener('click', async function(){
            const device = deviceSelect.value;
            const order = document.getElementById('orderInput').value.trim();
            if(!device || !order) return alert('Select device and enter order');
            try{
                const body = new URLSearchParams();
                body.append('action','send_order');
                body.append('device', device);
                body.append('order', order);
                const res = await fetch(api, { method: 'POST', body });
                const text = await res.text();
                if(res.ok) {
                    alert('Order sent');
                    document.getElementById('orderInput').value = '';
                    await refreshLog();
                } else {
                    alert('Send order failed: ' + text);
                }
            } catch(e){ alert('Error sending order'); }
        });

        // initial
        listDevices();
        // poll devices and log regularly
        setInterval(listDevices, 5000);
        setInterval(refreshLog, 3000);
    </script>
</body>
</html>
