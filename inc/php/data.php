<?php
header('Content-Type: text/plain');

// Simple file-based store for devices and messages
$preferredPath = __DIR__ . '/chat_store.json';

// Determine a writable storage file. Try preferred path first, fall back to system temp dir.
$storeFile = $preferredPath;

function init_store_file(&$path) {
	// If file exists and is writable, keep it.
	if (file_exists($path) && is_writable($path)) return true;

	// If directory is writable (can create file), try to create the file.
	$dir = dirname($path);
	if (is_writable($dir)) {
		// attempt to create the file if missing
		if (!file_exists($path)) {
			$ok = @file_put_contents($path, json_encode(['devices'=>[], 'messages'=>[]], JSON_PRETTY_PRINT), LOCK_EX);
			if ($ok === false) return false;
		}
		return is_writable($path);
	}

	// Fallback: use system temp directory (usually writable by webserver)
	$tmp = sys_get_temp_dir() . '/chat_store_' . md5(__DIR__) . '.json';
	if (!file_exists($tmp)) {
		$ok = @file_put_contents($tmp, json_encode(['devices'=>[], 'messages'=>[]], JSON_PRETTY_PRINT), LOCK_EX);
		if ($ok === false) return false;
	}
	$path = $tmp;
	return is_writable($path);
}

function load_store($path) {
	if (!file_exists($path)) return ['devices'=>[], 'messages'=>[]];
	$json = @file_get_contents($path);
	if ($json === false) return ['devices'=>[], 'messages'=>[]];
	$data = json_decode($json, true);
	if (!is_array($data)) return ['devices'=>[], 'messages'=>[]];
	return $data;
}

function save_store($path, $data) {
	$tmp = $path . '.tmp';
	$ok = @file_put_contents($tmp, json_encode($data, JSON_PRETTY_PRINT), LOCK_EX);
	if ($ok === false) return false;
	// atomic rename
	if (!@rename($tmp, $path)) {
		// fallback to direct write
		$ok2 = @file_put_contents($path, json_encode($data, JSON_PRETTY_PRINT), LOCK_EX);
		return $ok2 !== false;
	}
	return true;
}

// ensure storage file is usable (may change $storeFile to fallback path)
if (!init_store_file($storeFile)) {
	http_response_code(500);
	echo "ERROR: Storage not writable on server.\n";
	exit;
}

$action = $_GET['action'] ?? $_POST['action'] ?? null;
$device = $_GET['device'] ?? $_POST['device'] ?? null;
$msg = $_GET['msg'] ?? $_POST['msg'] ?? null;

$store = load_store($storeFile);

if ($action === 'connect' && $device) {
	// register or mark device online
	$store['devices'][$device] = ['connected' => true, 'lastSeen' => time()];
	if (!isset($store['messages'][$device])) $store['messages'][$device] = [];
	save_store($storeFile, $store);
	echo "OK";
	exit;
}

if ($action === 'heartbeat' && $device) {
	if (!isset($store['devices'][$device])) $store['devices'][$device] = ['connected'=>true, 'lastSeen'=>time()];
	$store['devices'][$device]['lastSeen'] = time();
	save_store($storeFile, $store);
	echo "OK";
	exit;
}

if ($action === 'list_devices') {
	header('Content-Type: application/json');
	echo json_encode($store['devices']);
	exit;
}

if ($action === 'send' && $device && $msg !== null) {
	// send from web -> device (pending)
	if (!isset($store['messages'][$device])) $store['messages'][$device] = [];
	$entry = ['from' => 'web', 'msg' => $msg, 'time' => time()];
	$store['messages'][$device][] = $entry;
	save_store($storeFile, $store);
	echo "SENT";
	exit;
}

if ($action === 'get_messages' && $device) {
	// device polls for pending web->device messages; return them then clear
	$pending = [];
	if (isset($store['messages'][$device])) {
		foreach ($store['messages'][$device] as $m) {
			if ($m['from'] === 'web') $pending[] = $m['msg'];
		}
		// remove web->device messages while keeping device->web logs
		$store['messages'][$device] = array_values(array_filter($store['messages'][$device], function($m){ return $m['from'] !== 'web'; }));
		save_store($storeFile, $store);
	}
	// return plain text, one message per line
	if (empty($pending)) {
		echo "";
	} else {
		echo implode("\n", $pending);
	}
	exit;
}

if ($action === 'post_response' && $device && $msg !== null) {
	// device posts a response back to web
	if (!isset($store['messages'][$device])) $store['messages'][$device] = [];
	$entry = ['from' => 'device', 'msg' => $msg, 'time' => time()];
	$store['messages'][$device][] = $entry;
	save_store($storeFile, $store);
	echo "OK";
	exit;
}

if ($action === 'reset') {
	// clear all devices and messages
	$store = ['devices' => [], 'messages' => []];
	if (save_store($storeFile, $store)) {
		echo "OK";
	} else {
		http_response_code(500);
		echo "ERROR";
	}
	exit;
}

if ($action === 'get_log' && $device) {
	header('Content-Type: application/json');
	$log = $store['messages'][$device] ?? [];
	echo json_encode($log);
	exit;
}

// default: show a simple help text
echo "Usage:\n";
echo " - ?action=connect&device=DEV    -> register device online\n";
echo " - ?action=heartbeat&device=DEV  -> update lastSeen\n";
echo " - ?action=list_devices         -> JSON list of devices\n";
echo " - ?action=send&device=DEV&msg=..-> send message from web to device\n";
echo " - ?action=get_messages&device=DEV -> device polls pending messages (plain text lines)\n";
echo " - ?action=post_response&device=DEV&msg=.. -> device posts response to web\n";
echo " - ?action=get_log&device=DEV   -> get JSON log of messages for device\n";
?>