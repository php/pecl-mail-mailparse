--TEST--
mailparse_msg_free() on a child part leaves the parent intact
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--FILE--
<?php
$root = mailparse_msg_create();
mailparse_msg_parse($root, "MIME-Version: 1.0\r\n"
    . "Content-Type: multipart/mixed; boundary=x\r\n\r\n"
    . "--x\r\nContent-Type: text/plain\r\n\r\nbody\r\n");

$child = mailparse_msg_get_part($root, '1.1');
var_dump(mailparse_msg_free($child));

mailparse_msg_parse($root, "more\r\n--x\r\nContent-Type: text/html\r\n\r\n<p>\r\n--x--\r\n");
var_dump(mailparse_msg_get_structure($root));
var_dump(mailparse_msg_get_part_data(mailparse_msg_get_part($root, '1.1'))['content-type']);
var_dump(mailparse_msg_free($root));
?>
--EXPECT--
bool(true)
array(3) {
  [0]=>
  string(1) "1"
  [1]=>
  string(3) "1.1"
  [2]=>
  string(3) "1.2"
}
string(10) "text/plain"
bool(true)
