--TEST--
Check mailparse_rfc822_parse_addresses
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
$addresses = <<<EOD
"Giant; \"Big\" Box" <sysservices@example.net>
EOD;

$addressesParsed = mailparse_rfc822_parse_addresses($addresses);

echo $addressesParsed[0]['display'];
?>
--EXPECT--
Giant; "Big" Box
