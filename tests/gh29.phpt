--TEST--
GH issue #29 Segmentation fault with ISO-2022-JP Subject header
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--FILE--
<?php

$data = <<<'EOF'
From: aaa@example.jp
To: bbb@example.jp
Subject: ヘッダあり
Content-type: text/plain; charset=ISO-2022-JP
Content-transfer-encoding: 7bit

お疲れ様です、テストメールです。

以上、よろしくお願い致します。
EOF;

$resource = mailparse_msg_create();

$r = mailparse_msg_parse($resource, $data);
echo 'ok', PHP_EOL;

mailparse_msg_free($resource);

exit(0);
?>
--EXPECTF--
ok
