--TEST--
mailparse_msg_free() does not close a non-mailparse resource
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
if (PHP_VERSION_ID < 80000) die("skip invalid resources raise TypeError only on PHP 8+");
?>
--FILE--
<?php
$fp = fopen('php://memory', 'r+');
try {
    mailparse_msg_free($fp);
} catch (TypeError $e) {
    echo $e::class, ": ", $e->getMessage(), PHP_EOL;
}
var_dump(fwrite($fp, "still open"));

$msg = mailparse_msg_create();
var_dump(mailparse_msg_free($msg));
var_dump(mailparse_msg_free($msg));
?>
--EXPECT--
TypeError: mailparse_msg_free(): supplied resource is not a valid mailparse_mail_structure resource
int(10)
bool(true)
bool(true)
