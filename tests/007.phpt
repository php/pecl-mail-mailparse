--TEST--
Check RFC822 Conformance
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
$addresses = array(
	"\":sysmail\"@ Some-Group. Some-Org, Muhammed.(I am the greatest) Ali @(the)Vegas.WBA",
	"\"strange\":\":sysmail\"@ Some-Group. Some-Org, Muhammed.(I am the greatest) Ali @(the)Vegas.WBA;"
);

foreach($addresses as $address) {
	$parsed = mailparse_rfc822_parse_addresses($address);
	foreach($parsed as $pair) {
		print $pair["display"] . "\n";
		print $pair["address"] . "\n";
		if ($pair["is_group"]) {
			$sub = mailparse_rfc822_parse_addresses(substr($pair["address"], 1, strlen($pair["address"]) - 2));
			foreach($sub as $subpair) {
				print "   " . $subpair["address"] . "\n";
			}
		}
	}
	print "...\n";
}

?>
--EXPECT--
:sysmail@Some-Group.Some-Org
":sysmail"@Some-Group.Some-Org
I am the greatest the
Muhammed.Ali@Vegas.WBA
...
strange
:":sysmail"@Some-Group.Some-Org,Muhammed.Ali@Vegas.WBA;
   ":sysmail"@Some-Group.Some-Org
   Muhammed.Ali@Vegas.WBA
...

