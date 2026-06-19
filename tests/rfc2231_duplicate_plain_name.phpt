--TEST--
A plain parameter repeating an RFC2231 encoded name does not leak the name
--SKIPIF--
<?php if (!extension_loaded("mailparse")) print "skip"; ?>
--FILE--
<?php
/* "URL*0=a" opens an RFC2231 continuation named URL; the following plain
 * "URL=b" repeats that base name. The duplicate name string used to leak. */
$m = mailparse_msg_create();
mailparse_msg_parse($m, "Content-Type: text/plain; URL*0=\"a\"; URL=\"b\"\r\n\r\nbody\r\n");
$d = mailparse_msg_get_part_data($m);
var_dump($d["content-type"]);
var_dump($d["content-url"]);
mailparse_msg_free($m);
echo "done\n";
?>
--EXPECT--
string(10) "text/plain"
string(1) "a"
done
