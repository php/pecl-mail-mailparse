--TEST--
Check mailparse_determine_best_xfer_encoding
--SKIPIF--
<?php
/* vim600: sw=4 ts=4 fdm=marker syn=php
*/
if (!extension_loaded("mailparse")) print "skip"; ?>
--FILE--
<?php
$files = new RecursiveIteratorIterator(new RecursiveDirectoryIterator(dirname(__FILE__) . "/testdata"));
foreach ($files as $file) {
	if (strrpos($file, '.txt') !== false) {
		var_dump(mailparse_determine_best_xfer_encoding(fopen($file->getRealPath(), 'r')));
	}
}
?>
--EXPECT--
string(4) "7bit"
string(6) "BASE64"
string(4) "7bit"
string(4) "7bit"
string(4) "7bit"
string(4) "7bit"
