--TEST--
GH issue #44 (Segmentation fault in mimepart resource destructor during shutdown)
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--FILE--
<?php
/* Generate a multipart message with >300 parts to trigger MAXPARTS.
 * This exercises the mailparse_msg_parse_file error path which calls
 * php_mimepart_free() directly, leaving child resources in the resource
 * list. Without the fix, these dangling resources cause a use-after-free
 * during shutdown in zend_close_rsrc_list. */

$boundary = "test_boundary";
$mime = "Content-Type: multipart/mixed; boundary=\"$boundary\"\r\n\r\n";
for ($i = 0; $i < 302; $i++) {
    $mime .= "--$boundary\r\nContent-Type: text/plain\r\n\r\npart $i\r\n";
}
$mime .= "--$boundary--\r\n";

$tmpfile = tempnam(sys_get_temp_dir(), 'mp_gh44_');
file_put_contents($tmpfile, $mime);

/* Parse the oversized message - triggers "MIME message too complex" warning
 * and the error path calls php_mimepart_free(part) directly, freeing the
 * mimepart struct but leaving child zend_resource entries in the list */
$result = @mailparse_msg_parse_file($tmpfile);
echo "parse_file returned: " . var_export($result, true) . "\n";

/* Allocate many messages to reuse freed memory from the failed parse above.
 * This increases the chance that the dangling resource pointers from the
 * failed parse will reference reused/corrupted memory at shutdown. */
$msgs = [];
for ($i = 0; $i < 50; $i++) {
    $m = mailparse_msg_create();
    mailparse_msg_parse($m, "Content-Type: multipart/mixed; boundary=\"b\"\r\n\r\n" .
        "--b\r\nContent-Type: text/plain\r\n\r\nhello\r\n" .
        "--b\r\nContent-Type: text/html\r\n\r\n<b>hi</b>\r\n--b--\r\n");
    $msgs[] = $m;
}

/* Also test normal multipart parsing and cleanup */
$msg = mailparse_msg_create();
mailparse_msg_parse($msg, "Content-Type: multipart/mixed; boundary=\"x\"\r\n\r\n" .
    "--x\r\nContent-Type: text/plain\r\n\r\nhello\r\n--x--\r\n");
$struct = mailparse_msg_get_structure($msg);
echo "structure: " . implode(", ", $struct) . "\n";

unlink($tmpfile);
echo "ok\n";
?>
--EXPECT--
parse_file returned: false
structure: 1, 1.1
ok
