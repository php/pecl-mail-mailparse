--TEST--
OO API Segfault when opening a file is not possible
--SKIPIF--
<?php 
/* vim600: sw=4 ts=4 fdm=marker syn=php
*/
if (!extension_loaded("mailparse")) print "skip"; ?>
--POST--
--GET--
--FILE--
<?php
error_reporting(0);
$msg = new MimeMessage("file", md5(uniqid("nothere")));
if (is_resource($msg))
	echo "Err??";
else
	echo "OK";
?>
--EXPECT--
OK
