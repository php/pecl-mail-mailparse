--TEST--
GH issue #19 (Segmentation fault with PHP 8.1 in extract_body using MAILPARSE_EXTRACT_RETURN)
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--FILE--
<?php
$original = <<<TXT
From: "Saved by Windows Internet Explorer 8"
Subject:
Date: Wed, 14 May 2014 12:34:56 +0200
MIME-Version: 1.0
Content-Type: text/html;
    charset="Windows-1252"
Content-Transfer-Encoding: quoted-printable
X-MimeOLE: Produced By Microsoft MimeOLE V6.1.7601.17609

<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">
<HTML><HEAD>
<META content=3D"text/html; charset=3Dwindows-1252" =
http-equiv=3DContent-Type>
<META name=3DGENERATOR content=3D"MSHTML 8.00.7601.18404"></HEAD>
<BODY></BODY></HTML>

TXT;

$msg      = new \MimeMessage("var", $original);
$contents = $msg->extract_body(\MAILPARSE_EXTRACT_RETURN);

var_dump($contents);

exit(0);
?>
--EXPECTF--
string(%d) "<!DOCTYPE %A"
