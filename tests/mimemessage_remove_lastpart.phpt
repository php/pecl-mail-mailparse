--TEST--
Freeing a part removed with MimeMessage::remove() leaves its former parent intact
--SKIPIF--
<?php if (!extension_loaded("mailparse")) print "skip"; ?>
--FILE--
<?php
$data = "MIME-Version: 1.0\r\n"
    . "Content-Type: multipart/mixed; boundary=x\r\n\r\n"
    . "--x\r\nContent-Type: text/plain\r\n\r\nbody\r\n";

$root = new MimeMessage('var', $data);
$child = $root->get_child(1);
$child->remove();

var_dump(mailparse_msg_free(((array)$child)[0]));
$res = ((array)$root)[0];
var_dump(mailparse_msg_parse($res, "more\r\n"));
var_dump(mailparse_msg_get_structure($res));
?>
--EXPECT--
bool(true)
bool(true)
array(1) {
  [0]=>
  string(1) "1"
}
