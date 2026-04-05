--TEST--
GH issue #20 (Unexpected parsed value of content-id with parentheses)
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--FILE--
<?php
$mime = "Content-Type: image/png\r\n" .
    "Content-Transfer-Encoding: base64\r\n" .
    "Content-ID: <Facebook_32x32(1)_aa284ba9-f148-4698-9c1f-c8e92bdb842e.png>\r\n" .
    "\r\n" .
    "iVBOR\r\n";

$resource = mailparse_msg_create();
mailparse_msg_parse($resource, $mime);

$part = mailparse_msg_get_part($resource, 1);
$data = mailparse_msg_get_part_data($part);

echo "content-id: " . $data['content-id'] . "\n";

/* Also test bare content-id without angle brackets */
$mime2 = "Content-Type: image/png\r\n" .
    "Content-Transfer-Encoding: base64\r\n" .
    "Content-ID: Facebook_32x32(1)_test.png\r\n" .
    "\r\n" .
    "iVBOR\r\n";

$resource2 = mailparse_msg_create();
mailparse_msg_parse($resource2, $mime2);

$part2 = mailparse_msg_get_part($resource2, 1);
$data2 = mailparse_msg_get_part_data($part2);

echo "content-id bare: " . $data2['content-id'] . "\n";

/* Standard RFC-compliant content-id */
$mime3 = "Content-Type: image/png\r\n" .
    "Content-ID: <part1@example.com>\r\n" .
    "\r\n" .
    "iVBOR\r\n";

$resource3 = mailparse_msg_create();
mailparse_msg_parse($resource3, $mime3);

$part3 = mailparse_msg_get_part($resource3, 1);
$data3 = mailparse_msg_get_part_data($part3);

echo "content-id rfc: " . $data3['content-id'] . "\n";

/* RFC-compliant content-id with trailing comment */
$mime4 = "Content-Type: image/png\r\n" .
    "Content-ID: <part1@example.com> (comment)\r\n" .
    "\r\n" .
    "iVBOR\r\n";

$resource4 = mailparse_msg_create();
mailparse_msg_parse($resource4, $mime4);

$part4 = mailparse_msg_get_part($resource4, 1);
$data4 = mailparse_msg_get_part_data($part4);

echo "content-id trailing comment: " . $data4['content-id'] . "\n";

echo "ok\n";
?>
--EXPECT--
content-id: Facebook_32x32(1)_aa284ba9-f148-4698-9c1f-c8e92bdb842e.png
content-id bare: Facebook_32x32(1)_test.png
content-id rfc: part1@example.com
content-id trailing comment: part1@example.com
ok
