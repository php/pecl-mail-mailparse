--TEST--
Check handling of multiple To headers
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
if (!extension_loaded("mailparse")) @dl("mailparse.so");
$text = <<<EOD
To: fred@bloggs.com
To: wez@thebrainroom.com

hello, this is some text=hello.
EOD;

$mime = mailparse_msg_create();
mailparse_msg_parse($mime, $text);
$data = mailparse_msg_get_part_data($mime);
echo $data["headers"]["to"];
?>
--EXPECT--
fred@bloggs.com, wez@thebrainroom.com
