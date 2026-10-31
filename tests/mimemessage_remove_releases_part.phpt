--TEST--
MimeMessage::remove() releases the removed part
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
if (getenv("USE_ZEND_ALLOC") === "0") die("skip memory_get_usage() needs the Zend allocator");
?>
--FILE--
<?php
$data = "MIME-Version: 1.0\r\n"
    . "Content-Type: multipart/mixed; boundary=x\r\n\r\n"
    . "--x\r\nContent-Type: text/plain\r\n\r\none\r\n"
    . "--x\r\nContent-Type: text/html\r\n\r\ntwo\r\n--x--\r\n";

$root = new MimeMessage('var', $data);
var_dump($root->get_child_count());
$child = $root->get_child(1);
$child->remove();
var_dump($root->get_child_count());
var_dump($child->get_parent());
var_dump($root->get_child(1)->data['content-type']);
unset($child, $root);

$warmup = new MimeMessage('var', $data);
$warmup->get_child(1)->remove();
unset($warmup);

$before = memory_get_usage();
for ($i = 0; $i < 100; $i++) {
    $msg = new MimeMessage('var', $data);
    $msg->get_child(1)->remove();
    unset($msg);
}
var_dump(memory_get_usage() - $before);
?>
--EXPECT--
int(3)
int(2)
NULL
string(9) "text/html"
int(0)
