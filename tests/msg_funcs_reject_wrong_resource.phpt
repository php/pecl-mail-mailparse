--TEST--
mailparse_msg_* reject a non-mimepart resource instead of crashing
--SKIPIF--
<?php if (!extension_loaded("mailparse")) print "skip"; ?>
--FILE--
<?php
/* Passing a wrong-type resource (a plain stream handle) used to dereference a
 * NULL mimepart and segfault. It must now fail cleanly: a TypeError on PHP 8,
 * a warning + false on PHP 7. Either way the process survives. */
$fp = tmpfile();

foreach (array("mailparse_msg_parse", "mailparse_msg_get_structure",
		"mailparse_msg_get_part_data", "mailparse_msg_get_part",
		"mailparse_msg_extract_part") as $fn) {
	try {
		@$fn($fp, "x");
	} catch (\Throwable $e) {
		/* TypeError on PHP 8 */
	}
}

fclose($fp);
echo "survived\n";
?>
--EXPECT--
survived
