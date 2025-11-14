Chat Library & Integration - Usage Guide
=====================================

Overview
--------
This document explains how the lightweight chat/order system works and how to use the provided PHP library (`chat_lib.php`), the HTTP endpoints in `data.php`, the admin UI in `test_control.php`, and the ESP32 example sketch `Homeassistant.ino`.

Goal
----
Provide a simple, reusable API so other PHP pages can send text messages or structured orders to devices (ESP32), and the devices can report back status and confirmations.

Files
-----
- `chat_lib.php` - server-side library with functions to manage devices and messages.
- `data.php` - HTTP endpoint wrapper that maps query/post actions to library functions.
- `test_control.php` - admin UI to manually send messages/orders, view devices and logs.
- `chat_store.json` - optional storage file (created if writable); otherwise the library uses a temp file.
- `Homeassistant.ino` - ESP32 sketch example showing how a device registers, polls messages, parses `ORDER|...` messages, and posts responses.

Quick API summary (via `data.php`)
---------------------------------
- `?action=connect&device=DEV` — register device online
- `?action=heartbeat&device=DEV` — update lastSeen
- `?action=list_devices` — returns JSON list of devices
- `?action=send&device=DEV&msg=...` — queue a plain text message for device
- `?action=send_order&device=DEV` — POST (or GET) with `order=k=v;k2=v2` to queue an ORDER message
- `?action=get_messages&device=DEV` — device polls for pending web->device messages (plain text lines); use `?format=json` for structured entries
- `?action=post_response&device=DEV&msg=...` — device posts a response back to server
- `?action=get_log&device=DEV` — get JSON log of all messages for device
- `?action=reset` — clears all devices and messages

Using the library from another PHP page
--------------------------------------
1. Include the library

```php
require_once __DIR__ . '/inc/php/chat_lib.php';
```

2. Send a plain text message

```php
chat_add_message('7C:2C:67:54:92:2C', 'web', 'Hello device', 'text');
```

3. Send a structured order (recommended server-side)

```php
chat_send_order('7C:2C:67:54:92:2C', [
  'action' => 'move',
  'from' => '1',
  'to' => '2',
  'switch1' => 'on',
]);
```

This encodes into a single queued message: `ORDER|action=move;from=1;to=2;switch1=on`

4. Read device logs

```php
$log = chat_get_log('7C:2C:67:54:92:2C');
foreach($log as $entry) {
  // $entry['from'], $entry['type'], $entry['msg'], $entry['time']
}
```

Admin UI (`test_control.php`)
--------------------------------
- Use this page to view devices, send free-text messages, and send structured orders using the `Order` input.
- The `Reset Store` button clears the stored state on the server.
- The UI calls the `data.php` endpoints and can be included by other admin pages.

ESP32 behavior (how to implement on device)
------------------------------------------
The example sketch (`Homeassistant.ino`) contains a simple flow:

1. Connect to WiFi and register the device by calling:
   `GET data.php?action=connect&device=<MAC>`

2. Poll for messages periodically:
   `GET data.php?action=get_messages&device=<MAC>`
   - By default the server returns plain text lines (one message per line).
   - If a message starts with `ORDER|`, the device should parse it as key=value pairs separated by `;`.

3. On receipt of an `ORDER|` message the device should:
   - Parse `k=v` pairs into action parameters (e.g. `action=move`, `from=1`, `to=2`, `switch1=on`)
   - Execute the required sequence (motor control, switching tracks).
   - Post a response/confirmation back to server: `data.php?action=post_response&device=<MAC>&msg=...`

4. Periodically send a heartbeat:
   `GET data.php?action=heartbeat&device=<MAC>`

Message formats
---------------
- Text messages: any string (type `text`).
- Orders: a single string beginning with `ORDER|` followed by `k=v` pairs separated by `;`. Example:
  `ORDER|action=move;from=1;to=2;switch1=on;speed=50`
- Device responses: simple text or an `ExecutedOrder|...` acknowledgement.

Example ESP32 serial log (expected)
-----------------------------------
```
Starting ESP32 chat client
Connecting to WiFi... (dots)
WiFi connected: 192.168.2.65
Device ID: 7C:2C:67:54:92:2C
Register response: OK
--- Received commands ---
CMD: ORDER|action=move;from=1;to=2;switch1=on
  -> action = move
  -> from = 1
  -> to = 2
  -> switch1 = on
Executing order...
Posted response: OK
```

Server-side example (curl)
--------------------------
- Send a plain text message:

```bash
curl "http://yourserver/Train/inc/php/data.php?action=send&device=7C:2C:67:54:92:2C&msg=Hello"
```

- Send an order (POST):

```bash
curl -X POST -d "action=send_order" -d "device=7C:2C:67:54:92:2C" -d "order=action=move;from=1;to=2;switch1=on" \
  "http://yourserver/Train/inc/php/data.php"
```

Security & deployment notes
---------------------------
- By default endpoints are not authenticated. If the server is exposed beyond your LAN, restrict access or add a token.
- The library will try to use `chat_store.json` in the same folder; if not writable it falls back to a temp file in the system temp directory.
- Make sure your webserver/PHP process can write to the storage if you want persistence across reboots.

Troubleshooting
---------------
- If the ESP32 shows a PHP warning about permissions, create `chat_store.json` and give the webserver user write permission, or rely on the temp-file fallback.
- To check which storage file is used, look in the temp dir for `chat_store_*.json` or inspect `chat_store.json` in the PHP directory.

---
End of guide
