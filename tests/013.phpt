--TEST--
Check mailparse_mimemessage_extract_uue
--SKIPIF--
<?php
/* vim600: sw=4 ts=4 fdm=marker syn=php
*/
if (!extension_loaded("mailparse")) print "skip"; ?>
--FILE--
<?php
$msg = mailparse_msg_parse_file(dirname(__FILE__) . "/testdata/oeuue");
var_dump($msg);
mailparse_msg_free($msg);
?>
--EXPECTF--
resource(%d) of type (mailparse_mail_structure)
