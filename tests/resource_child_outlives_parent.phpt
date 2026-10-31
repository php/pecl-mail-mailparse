--TEST--
A child mailparse resource can safely outlive its parent
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--FILE--
<?php
$data = "MIME-Version: 1.0\r\n"
    . "Content-Type: multipart/mixed; boundary=x\r\n\r\n"
    . "--x\r\nContent-Type: text/plain\r\n\r\nbody\r\n";

$root = mailparse_msg_create();
mailparse_msg_parse($root, $data);
$child = mailparse_msg_get_part($root, '1.1');

var_dump(mailparse_msg_free($root));
var_dump(mailparse_msg_parse($child, "X-Test: value\r\n"));
$info = mailparse_msg_get_part_data($child);
var_dump($info['content-type'], $info['line-count']);
var_dump(mailparse_msg_free($child));
?>
--EXPECT--
bool(true)
bool(true)
string(10) "text/plain"
int(4)
bool(true)
