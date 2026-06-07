--TEST--
Deeply nested MIME parts are rejected instead of overflowing the stack
--SKIPIF--
<?php if (!extension_loaded("mailparse")) print "skip"; ?>
--FILE--
<?php
$msg = str_repeat("Content-Type: message/rfc822\n\n", 1000);
$m = mailparse_msg_create();
var_dump(mailparse_msg_parse($m, $msg));
/* walking the (capped) tree must not crash */
mailparse_msg_get_structure($m);
echo "done\n";
mailparse_msg_free($m);
?>
--EXPECTF--
Warning: mailparse_msg_parse(): MIME message too deeply nested in %s on line %d
bool(false)
done
