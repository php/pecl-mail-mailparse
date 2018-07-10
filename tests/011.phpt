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
$names = array();
foreach ($files as $file) {
	if (strrpos($file->getFileName(), '.txt') !== false) {
		$names[] = $file->getRealPath();
	}
}
sort($names);
foreach ($names as $name) {
	var_dump(mailparse_determine_best_xfer_encoding(fopen($name, 'r')));
}
?>
--EXPECT--
string(4) "7bit"
string(4) "7bit"
string(6) "BASE64"
string(4) "7bit"
string(4) "7bit"
string(4) "7bit"
string(4) "7bit"
