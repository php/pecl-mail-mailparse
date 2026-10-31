--TEST--
mailparse_msg_free() refuses to free a part from inside its own extract callback
--SKIPIF--
<?php if (!extension_loaded("mailparse")) die("skip mailparse extension not available"); ?>
--FILE--
<?php
$data = "MIME-Version: 1.0\r\nContent-Type: text/plain\r\nContent-Transfer-Encoding: base64\r\n\r\n"
    . chunk_split(base64_encode(str_repeat("A", 20000)));
$msg = mailparse_msg_create();
mailparse_msg_parse($msg, $data);

$len = 0;
$first = true;
mailparse_msg_extract_part($msg, $data, function ($chunk) use ($msg, &$len, &$first) {
    if ($first) {
        $first = false;
        var_dump(mailparse_msg_free($msg));
    }
    $len += strlen($chunk);
});
var_dump($len);
var_dump(mailparse_msg_free($msg));
?>
--EXPECTF--
Warning: mailparse_msg_free(): Cannot free a message part while it is being extracted in %s on line %d
bool(false)
int(20000)
bool(true)
