--TEST--
GH issue #21 (Segmentation fault with mailparse_msg_create())
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--FILE--
<?php

$data = <<<'EOF'
Date: Wed, 29 Jun 2022 19:14:02 +0000
To: dmarc-noreply@linkedin.com
From: dmarc-noreply@linkedin.com
Content-Type: multipart/report; report-type=feedback-report;
    boundary="part1_boundary"

--part1_boundary
Content-Type: text/plain; charset="US-ASCII"
Content-Transfer-Encoding: 7bit

This is an email abuse report for an email message received from IP x.x.x.x on Wed, 29 Jun 2022 19:14:02 +0000.
The message below did not meet the sending domain's dmarc policy.
The message below could have been accepted or rejected depending on policy.
For more information about this format please see http://tools.ietf.org/html/rfc6591 .

--part1_boundary
Content-Type: message/feedback-report

Feedback-Type: auth-failure
User-Agent: Lua/1.0
Version: 1.0

--part1_boundary
Content-Type: message/rfc822
Content-Disposition: inline

Date: Wed, 29 Jun 2022 20:13:58 +0100
From: "Example" <mail@example.com>
To: "LinkedIn" <foo@bounce.linkedin.com>
Content-Type: multipart/alternative;
	boundary="part2_boundary"

--part2_boundary
Content-Type: text/plain; charset=UTF-8
Content-Transfer-Encoding: quoted-printable

foo

--part2_boundary
Content-Type: text/html; charset=UTF-8
Content-Transfer-Encoding: quoted-printable

<html>foo</html>

--part2_boundary--

--part1_boundary--
EOF;

$resource = mailparse_msg_create();

$r = mailparse_msg_parse($resource, $data);
echo 'ok', PHP_EOL;

mailparse_msg_free($resource);

exit(0);
?>
--EXPECTF--
ok
