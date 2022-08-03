--TEST--
GH issue #22 (Segmentation fault with mailparse_msg_create())
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--FILE--
<?php

$data = <<<'EOF'
X-Original-To: plus@protonmail.dev
Received: from mail-test.protonmail.dev (mail-test.protonmail.dev [74.125.82.50])
 by mail9i.protonmail.dev for <test@protonmail.dev>; Mon, 30 Apr 2018 13:03:00 +0000 (UTC)
To: ProtonMail Test <plus@protonmail.dev>
Subject: Buggy message
From: dummyaddress@domain.com
Date: Tue, 02 Aug 2022 20:53:51 -0400
MIME-Version: 1.0
Content-Type: multipart/mixed;
        boundary="MCBoundary=_12208022055093421"

--MCBoundary=_12208022055093421
Content-Type: multipart/related;
        boundary="MCBoundary=_12208022055093431"

--MCBoundary=_12208022055093431
Content-Transfer-Encoding: quoted-printable
Content-Type: text/html; charset=UTF-8

hello part 1

--MCBoundary=_12208022055093431--

--MCBoundary=_12208022055093421
Content-Type: message/rfc822;
        name="a name"
Content-Disposition: inline;
        filename="a name"

Message-Id: <attach-12208022055093351@localhost>
Date: Tue, 02 Aug 2022 20:53:51 -0400
From: some@one.com
To: someone@protonmail.com
Subject: a subject
Content-Type: multipart/alternative;
        boundary="MCBoundary=_12208022055093381"

--MCBoundary=_12208022055093381
Content-Type: text/plain; charset=UTF-8
Content-Transfer-Encoding: quoted-printable

hello other part

--MCBoundary=_12208022055093381
Content-Type: text/html; charset=UTF-8
Content-Transfer-Encoding: quoted-printable

hello again

--MCBoundary=_12208022055093381--

--MCBoundary=_12208022055093421--
EOF;

$resource = mailparse_msg_create();

$r = @mailparse_msg_parse($resource, $data);
echo 'ok', PHP_EOL;

exit(0);
?>
--EXPECTF--
ok
