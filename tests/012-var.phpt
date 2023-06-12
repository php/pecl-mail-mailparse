--TEST--
Check mailparse_mimemessage_extract_uue (var mode)
--SKIPIF--
<?php
/* vim600: sw=4 ts=4 fdm=marker syn=php
*/
if (!extension_loaded("mailparse")) print "skip"; ?>
--FILE--
<?php
$var = file_get_contents(dirname(__FILE__) . "/testdata/oeuue");
$msg = new MimeMessage("var", $var);
var_dump( $msg->extract_uue(0, MAILPARSE_EXTRACT_RETURN));
?>
--EXPECT--
string(88) "FooBar - Baaaaa

Requirements: 
	o php with mailparse
    o virus scanner (optional)
	

"
