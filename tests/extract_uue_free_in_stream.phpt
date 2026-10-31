--TEST--
MimeMessage::extract_uue() keeps its part alive when the destination stream frees it
--SKIPIF--
<?php if (!extension_loaded("mailparse")) die("skip mailparse extension not available"); ?>
--FILE--
<?php
class FreeingStream
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
stream_wrapper_register('freeing', 'FreeingStream');

$data = "MIME-Version: 1.0\r\nContent-Type: text/plain\r\n\r\nbegin 644 a.txt\r\n"
    . convert_uuencode(str_repeat("B", 3000)) . "end\r\n";
$msg = new MimeMessage('var', $data);
$out = fopen('freeing://out', 'w');
FreeingStream::$onWrite = function () use ($msg) {
    var_dump(mailparse_msg_free(((array)$msg)[0]));
};
var_dump($msg->extract_uue(0, MAILPARSE_EXTRACT_STREAM, $out));
fclose($out);
var_dump(FreeingStream::$written);
?>
--EXPECTF--
Warning: mailparse_msg_free(): Cannot free a message part while it is being extracted in %s on line %d
bool(false)
bool(true)
int(3000)
