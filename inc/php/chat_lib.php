<?php
// chat_lib.php - lightweight chat/store library for devices

// Preferred storage file (can be overridden by server environment)
$CHAT_PREFERRED = __DIR__ . '/chat_store.json';

function chat_get_store_file() {
    global $CHAT_PREFERRED;
    static $store = null;
    if ($store !== null) return $store;
    $store = $CHAT_PREFERRED;
    // try to initialize similarly to previous logic
    if (file_exists($store) && is_writable($store)) return $store;
    $dir = dirname($store);
    if (is_writable($dir)) {
        if (!file_exists($store)) @file_put_contents($store, json_encode(['devices'=>[], 'messages'=>[]], JSON_PRETTY_PRINT), LOCK_EX);
        if (is_writable($store)) return $store;
    }
    $tmp = sys_get_temp_dir() . '/chat_store_' . md5(__DIR__) . '.json';
    if (!file_exists($tmp)) @file_put_contents($tmp, json_encode(['devices'=>[], 'messages'=>[]], JSON_PRETTY_PRINT), LOCK_EX);
    $store = $tmp;
    return $store;
}

function chat_load() {
    $path = chat_get_store_file();
    if (!file_exists($path)) return ['devices'=>[], 'messages'=>[]];
    $json = @file_get_contents($path);
    if ($json === false) return ['devices'=>[], 'messages'=>[]];
    $data = json_decode($json, true);
    if (!is_array($data)) return ['devices'=>[], 'messages'=>[]];
    if (!isset($data['devices'])) $data['devices'] = [];
    if (!isset($data['messages'])) $data['messages'] = [];
    return $data;
}

function chat_save($data) {
    $path = chat_get_store_file();
    $tmp = $path . '.tmp';
    $ok = @file_put_contents($tmp, json_encode($data, JSON_PRETTY_PRINT), LOCK_EX);
    if ($ok === false) return false;
    if (!@rename($tmp, $path)) {
        $ok2 = @file_put_contents($path, json_encode($data, JSON_PRETTY_PRINT), LOCK_EX);
        return $ok2 !== false;
    }
    return true;
}

function chat_list_devices() {
    $store = chat_load();
    return $store['devices'];
}

function chat_register_device($device) {
    $store = chat_load();
    $store['devices'][$device] = ['connected' => true, 'lastSeen' => time()];
    if (!isset($store['messages'][$device])) $store['messages'][$device] = [];
    return chat_save($store);
}

function chat_heartbeat($device) {
    $store = chat_load();
    if (!isset($store['devices'][$device])) $store['devices'][$device] = ['connected'=>true, 'lastSeen'=>time()];
    $store['devices'][$device]['lastSeen'] = time();
    return chat_save($store);
}

function chat_add_message($device, $from, $msg, $type = 'text') {
    $store = chat_load();
    if (!isset($store['messages'][$device])) $store['messages'][$device] = [];
    $entry = ['from'=>$from, 'type'=>$type, 'msg'=>$msg, 'time'=>time()];
    $store['messages'][$device][] = $entry;
    return chat_save($store);
}

function chat_get_pending_for_device($device) {
    $store = chat_load();
    $pending = [];
    if (!isset($store['messages'][$device])) $store['messages'][$device] = [];
    // collect messages from web -> device
    foreach ($store['messages'][$device] as $i => $m) {
        if ($m['from'] === 'web') $pending[] = $m;
    }
    // remove web->device messages while keeping device->web logs
    $store['messages'][$device] = array_values(array_filter($store['messages'][$device], function($m){ return $m['from'] !== 'web'; }));
    chat_save($store);
    return $pending;
}

function chat_post_response($device, $msg, $type='text') {
    return chat_add_message($device, 'device', $msg, $type);
}

function chat_get_log($device) {
    $store = chat_load();
    return $store['messages'][$device] ?? [];
}

function chat_reset() {
    $store = ['devices'=>[], 'messages'=>[]];
    return chat_save($store);
}

// helper to create an ORDER message from an associative array
function chat_send_order($device, $orderAssoc) {
    // encode as simple key=value; pairs; prefix with ORDER|
    $parts = [];
    foreach ($orderAssoc as $k => $v) {
        $k = str_replace([';', '|', '='], ['','', ''], (string)$k);
        $v = str_replace([';', '|', '='], ['','', ''], (string)$v);
        $parts[] = $k . '=' . $v;
    }
    $str = 'ORDER|' . implode(';', $parts);
    return chat_add_message($device, 'web', $str, 'order');
}

?>