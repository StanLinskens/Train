<?php
header('Content-Type: text/plain');

require_once __DIR__ . '/chat_lib.php';

$action = $_GET['action'] ?? $_POST['action'] ?? null;
$device = $_GET['device'] ?? $_POST['device'] ?? null;
$msg = $_GET['msg'] ?? $_POST['msg'] ?? null;

$store = chat_load();

if ($action === 'connect' && $device) {
	// register or mark device online
	chat_register_device($device);
	echo "OK";
	exit;
}

if ($action === 'heartbeat' && $device) {
	chat_heartbeat($device);
	echo "OK";
	exit;
}

if ($action === 'list_devices') {
	header('Content-Type: application/json');
	echo json_encode(chat_list_devices());
	exit;
}

if ($action === 'send' && $device && $msg !== null) {
	// send from web -> device (pending)
	chat_add_message($device, 'web', $msg, 'text');
	echo "SENT";
	exit;
}

if ($action === 'send_order' && $device) {
	// order passed as 'order' parameter (k=v;k2=v2) or JSON string
	$orderStr = $_POST['order'] ?? $_GET['order'] ?? null;
	if ($orderStr === null) {
		http_response_code(400);
		echo "Missing order";
		exit;
	}
	// ensure prefix
	if (!str_starts_with($orderStr, 'ORDER|')) $orderStr = 'ORDER|' . $orderStr;
	chat_add_message($device, 'web', $orderStr, 'order');
	echo "SENT_ORDER";
	exit;
}

if ($action === 'get_messages' && $device) {
	// device polls for pending web->device messages; return them then clear
	$pending = chat_get_pending_for_device($device);
	// if format=json requested, return full entries; otherwise return plain text lines
	$format = $_GET['format'] ?? $_POST['format'] ?? 'text';
	if ($format === 'json') {
		header('Content-Type: application/json');
		echo json_encode($pending);
	} else {
		$lines = [];
		foreach ($pending as $m) $lines[] = $m['msg'];
		echo implode("\n", $lines);
	}
	exit;
}

if ($action === 'post_response' && $device && $msg !== null) {
	// device posts a response back to web
	chat_post_response($device, $msg, 'text');
	echo "OK";
	exit;
}

if ($action === 'reset') {
	// clear all devices and messages
	if (chat_reset()) {
		echo "OK";
	} else {
		http_response_code(500);
		echo "ERROR";
	}
	exit;
}

if ($action === 'get_log' && $device) {
	header('Content-Type: application/json');
	echo json_encode(chat_get_log($device));
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