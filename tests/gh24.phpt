--TEST--
GH issue #24 (Segmentation fault with mailparse_msg_create())
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--FILE--
<?php

$data = <<<'EML'
Content-Type: multipart/mixed;
        boundary="MCBoundary=_12210121514003461"

--MCBoundary=_12210121514003461
Content-Type: message/rfc822

Content-Type: multipart/alternative;
        boundary="MCBoundary=_12210121514003451"

--MCBoundary=_12210121514003451
Content-Type: text/plain;

content

--MCBoundary=_12210121514003451--

--MCBoundary=_12210121514003461--
EML;

$resource = mailparse_msg_create();

$r = @mailparse_msg_parse($resource, $data);
echo 'ok', PHP_EOL;

mailparse_msg_free($resource);
exit(0);
?>
--EXPECTF--
ok
