--TEST--
Check mailparse_mimemessage_extract_uue (stream mode)
--SKIPIF--
<?php
/* vim600: sw=4 ts=4 fdm=marker syn=php
*/
if (!extension_loaded("mailparse")) print "skip"; ?>
--FILE--
<?php
$fp = fopen(dirname(__FILE__) . "/testdata/oeuue", "rb");
$msg = new MimeMessage("stream", $fp);
var_dump( $msg->extract_uue(0, MAILPARSE_EXTRACT_RETURN));
fclose($fp);
?>
--EXPECT--
string(88) "FooBar - Baaaaa

Requirements: 
	o php with mailparse
    o virus scanner (optional)
	

"
