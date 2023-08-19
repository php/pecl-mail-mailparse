--TEST--
Bug #81403 (mailparse_rfc822_parse_addresses drops escaped quotes)
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--XFAIL--
Fix reverted see GH-29 and GH-30
--FILE--
<?php
$address = '"Smith, Robert \"Bob\"" <user@domain.org>';
var_dump(mailparse_rfc822_parse_addresses($address));
?>
--EXPECT--
array(1) {
  [0]=>
  array(3) {
    ["display"]=>
    string(21) "Smith, Robert \"Bob\""
    ["address"]=>
    string(15) "user@domain.org"
    ["is_group"]=>
    bool(false)
  }
}
