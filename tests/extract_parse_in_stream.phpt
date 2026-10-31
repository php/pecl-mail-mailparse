--TEST--
mailparse_msg_parse() refuses to parse into a part while it is being extracted
--SKIPIF--
<?php if (!extension_loaded("mailparse")) die("skip mailparse extension not available"); ?>
--FILE--
<?php
class ParsingStream
{
    public $context;
    public static $onWrite;
    public static $written = 0;

    public function stream_open($path, $mode, $options, &$opened)
    {
        return true;
    }

    public function stream_write($data)
    {
        if (self::$onWrite) {
            $cb = self::$onWrite;
            self::$onWrite = null;
            $cb();
        }
        self::$written += strlen($data);
        return strlen($data);
    }
}
stream_wrapper_register('parsing', 'ParsingStream');

$data = "MIME-Version: 1.0\r\nContent-Type: multipart/mixed; boundary=x\r\n\r\n"
    . "--x\r\nContent-Type: text/plain\r\nContent-Transfer-Encoding: base64\r\n\r\n"
    . chunk_split(base64_encode(str_repeat("A", 30000)))
    . "\r\n--x--\r\n";
$root = mailparse_msg_create();
mailparse_msg_parse($root, $data);
$msg = mailparse_msg_get_part($root, '1.1');

$out = fopen('parsing://out', 'w');
stream_set_chunk_size($out, 1);
ParsingStream::$onWrite = function () use ($msg) {
    var_dump(mailparse_msg_parse($msg, str_repeat("Z", 200000)));
};
var_dump(mailparse_msg_extract_part($msg, $data, $out));
fclose($out);
var_dump(ParsingStream::$written);
?>
--EXPECTF--
Warning: mailparse_msg_parse(): Cannot parse into a message part while it is being extracted in %s on line %d
bool(false)
bool(true)
int(30000)
