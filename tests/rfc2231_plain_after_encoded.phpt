--TEST--
A plain parameter following an RFC2231 (name*N) parameter is not leaked
--SKIPIF--
<?php if (!extension_loaded("mailparse")) print "skip"; ?>
--FILE--
<?php
$m = mailparse_msg_create();
mailparse_msg_parse($m, "Content-Type: x/y; a*0=\"foo\"; a*1=\"bar\"; b=\"baz\"\n\nbody\n");
$d = mailparse_msg_get_part_data($m);
var_dump($d['content-a']);
var_dump($d['content-b']);
mailparse_msg_free($m);
echo "done\n";
?>
--EXPECT--
string(6) "foobar"
string(3) "baz"
done
