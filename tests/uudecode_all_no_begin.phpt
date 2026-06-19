--TEST--
mailparse_uudecode_all on input without a "begin" line does not leak
--SKIPIF--
<?php if (!extension_loaded("mailparse")) print "skip"; ?>
--FILE--
<?php
/* With no "begin" line the temp-file path string was never released. */
$fp = fopen("php://memory", "r+");
fwrite($fp, "plain text\nno uuencoded data here\n");
rewind($fp);
var_dump(mailparse_uudecode_all($fp));
fclose($fp);
echo "done\n";
?>
--EXPECT--
bool(false)
done
