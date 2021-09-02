--TEST--
Fix #74233 (Parsing multi Content-Disposition causes memory leak)
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--FILE--
<?php
$msg = <<<EOD
Subject:
To: root@example.com
mime-version: 1.0
Content-Type: multipart/mixed; boundary="=___BOUNDARY___"

--=___BOUNDARY___
Content-Type: text/plain; charset=ISO-2022-JP
Content-Transfer-Encoding: 7bit


--=___BOUNDARY___
Content-Type: text/plain; name="test.txt"
Content-Disposition: attachment; filename="test.txt"
Content-Disposition: attachment; filename="test2.txt"
Content-Transfer-Encoding: base64

dGVzdA==

--=___BOUNDARY___--

EOD;

new MimeMessage('var', $msg) !== false;
?>
--EXPECT--
