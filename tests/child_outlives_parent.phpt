--TEST--
A child MimeMessage can safely outlive its parent
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--FILE--
<?php
function firstChild(string $data): MimeMessage
{
    $root = new MimeMessage('var', $data);
    return $root->get_child(0);
}

$data = "MIME-Version: 1.0\r\n"
    . "Content-Type: multipart/mixed; boundary=x\r\n\r\n"
    . "--x\r\nContent-Type: text/plain\r\n\r\nbody\r\n--x--\r\n";

$child = firstChild($data);
var_dump($child instanceof MimeMessage);
var_dump($child->get_parent());
var_dump($child->get_child_count());
?>
--EXPECT--
bool(true)
NULL
int(0)
