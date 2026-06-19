--TEST--
MimeMessage::extract_uue() can extract uuencoded parts past the first
--SKIPIF--
<?php if (!extension_loaded("mailparse")) print "skip"; ?>
--FILE--
<?php
/* extract_uue() compared a part counter that it never incremented, so any
 * index greater than 0 returned NULL even when the part existed. */
function uue($name, $data) {
	return "begin 644 $name\n" . convert_uuencode($data) . "end\n\n";
}
$body = "intro line\n\n" . uue("a.txt", "FIRST") . "\n" . uue("b.txt", "SECOND");

$fp = fopen("php://memory", "r+");
fwrite($fp, $body);
rewind($fp);
$m = new MimeMessage("stream", $fp);
var_dump($m->extract_uue(0, MAILPARSE_EXTRACT_RETURN));
var_dump($m->extract_uue(1, MAILPARSE_EXTRACT_RETURN));
var_dump($m->extract_uue(2, MAILPARSE_EXTRACT_RETURN));
fclose($fp);
?>
--EXPECT--
string(5) "FIRST"
string(6) "SECOND"
NULL
