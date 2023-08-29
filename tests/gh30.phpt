--TEST--
GH issue #30 Segmentation fault with UTF-8 encoded X-MS-Iris-MetaData header
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--FILE--
<?php

$data = <<<'EOF'
From: aaa@example.com
To: bbb@example.com
Subject: Test
Content-Type: text/plain; charset=utf-8
Content-Transfer-Encoding: quoted-printable
X-MS-Iris-MetaData: =?utf-8?q?{=22Type=22=3Anull=2C=22Fields=22=3A{=22InstanceId=22=3A=2290232a75aacb417581ef2a67?=
 =?utf-8?q?0087e515=3A2=22=2C=22Market=22=3A=22en-gb=22=2C=22TopicId=22=3A=2220d3416a-aa91-4e58-a5?=
 =?utf-8?q?bc-4a2be2a64934=22=2C=22Timestamp=22=3A=228=2F13=2F2023?= 6:01:40
 AM"}}

Test
EOF;

$resource = mailparse_msg_create();

$r = mailparse_msg_parse($resource, $data);
echo 'ok', PHP_EOL;

mailparse_msg_free($resource);

exit(0);
?>
--EXPECTF--
ok
