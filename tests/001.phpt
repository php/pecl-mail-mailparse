--TEST--
Check for mailparse presence
--SKIPIF--
<?php
/* vim600: sw=4 ts=4 fdm=marker syn=php
*/
if (!extension_loaded("mailparse")) @dl("mailparse.so");
if (!extension_loaded("mailparse")) print "skip"; ?>
--POST--
--GET--
--FILE--
<?php 
echo "mailparse extension is available";
?>
--EXPECT--
mailparse extension is available
