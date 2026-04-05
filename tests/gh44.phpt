--TEST--
GH issue #44 (Segmentation fault in mimepart resource destructor during shutdown)
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--FILE--
<?php
$mime = "Content-Type: multipart/mixed; boundary=\"outer\"\r\n" .
    "\r\n" .
    "--outer\r\n" .
    "Content-Type: multipart/alternative; boundary=\"inner\"\r\n" .
    "\r\n" .
    "--inner\r\n" .
    "Content-Type: text/plain\r\n" .
    "\r\n" .
    "Hello plain\r\n" .
    "--inner\r\n" .
    "Content-Type: text/html\r\n" .
    "\r\n" .
    "<b>Hello html</b>\r\n" .
    "--inner--\r\n" .
    "--outer\r\n" .
    "Content-Type: application/octet-stream\r\n" .
    "Content-Transfer-Encoding: base64\r\n" .
    "\r\n" .
    "SGVsbG8=\r\n" .
    "--outer--\r\n";

/* Test 1: Parse multipart message, let resource cleanup happen at shutdown */
$msg1 = mailparse_msg_create();
mailparse_msg_parse($msg1, $mime);
$struct = mailparse_msg_get_structure($msg1);
echo "structure: " . implode(", ", $struct) . "\n";

/* Test 2: Parse and explicitly free */
$msg2 = mailparse_msg_create();
mailparse_msg_parse($msg2, $mime);
mailparse_msg_free($msg2);

/* Test 3: Get child parts (adds refcount), then let shutdown clean up */
$msg3 = mailparse_msg_create();
mailparse_msg_parse($msg3, $mime);
$parts = [];
foreach (mailparse_msg_get_structure($msg3) as $part_id) {
    $parts[] = mailparse_msg_get_part($msg3, $part_id);
}

/* Test 4: Multiple messages with interleaved resource handles */
for ($i = 0; $i < 5; $i++) {
    $m = mailparse_msg_create();
    mailparse_msg_parse($m, $mime);
}

echo "ok\n";
?>
--EXPECT--
structure: 1, 1.1, 1.1.1, 1.1.2, 1.2
ok
