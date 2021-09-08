--TEST--
Bug #73110 (Mails with unknown MIME version are treated as plain/text)
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--FILE--
<?php
$text = 'Mime-Version: 1.0;
Content-Type: multipart/Alternative; boundary="=====1473780076CSSSMTP_CLIENT=="

--=====1473780076CSSSMTP_CLIENT==
Content-Type: text/plain

Hello please pay attached invoice
--=====1473780076CSSSMTP_CLIENT==
Content-Type: text/html; charset=utf-8

Hello please pay attached invoice

--=====1473780076CSSSMTP_CLIENT==--';
$stream = fopen('php://memory', 'r+');
fwrite($stream, $text);
fseek($stream, 0);
$resource = mailparse_msg_create();
mailparse_msg_parse($resource, fread($stream, 10000));
$data = mailparse_msg_get_part_data(mailparse_msg_get_part($resource, 1));
var_dump($data['content-type']);
mailparse_msg_free($resource);
?>
--EXPECT--
string(21) "multipart/alternative"
