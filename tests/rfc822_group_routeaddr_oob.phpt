--TEST--
A route-addr inside a group does not read past the parsed address array
--SKIPIF--
<?php if (!extension_loaded("mailparse")) print "skip"; ?>
--FILE--
<?php
/* A group whose inner mailbox is an unterminated route-addr ("<" with no ">")
 * used to desync the counting and building passes of the address parser and
 * index one slot past the end of the allocated address array. */
foreach (array("<>:<", ":;:<", "a@b,<>:<") as $in) {
	$r = mailparse_rfc822_parse_addresses($in);
	var_dump(is_array($r));
}
echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
done
