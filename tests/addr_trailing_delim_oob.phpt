--TEST--
Trailing delimiters in an address list do not read past the token array
--SKIPIF--
<?php if (!extension_loaded("mailparse")) print "skip"; ?>
--FILE--
<?php
$r = mailparse_rfc822_parse_addresses("a@b,,");
var_dump($r[0]['address']);
$r = mailparse_rfc822_parse_addresses("grp: a@b,,");
var_dump($r[0]['is_group']);
echo "done\n";
?>
--EXPECT--
string(3) "a@b"
bool(true)
done
