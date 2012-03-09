--TEST--
Check quoted-printable encoding generates S/MIME safe content
--SKIPIF--
<?php
/* vim600: sw=4 ts=4 fdm=marker syn=php
*/
if (!extension_loaded("mailparse")) @dl("mailparse.so");
if (!extension_loaded("mailparse")) print "skip"; ?>
--POST--
--GET--
--FILE--
<?php 
if (!extension_loaded("mailparse")) @dl("mailparse.so");
$text = <<<EOD
To: fred@bloggs.com

blah blah blah From blah $ " & £ blah blah blah blah blah
From the first of the month, things will be different!
blah blah blah From blah
Frome is a town in Somerset.
EOD;

$fp = tmpfile();
fwrite($fp, $text);
rewind($fp);

$fpdest = tmpfile();

mailparse_stream_encode($fp, $fpdest, "quoted-printable");

rewind($fpdest);

fpassthru($fpdest);

fclose($fp);
fclose($fpdest);
?>
--EXPECT--
To: fred@bloggs.com

blah blah blah From blah $ " & =A3 blah blah blah blah blah
=46rom the first of the month, things will be different!
blah blah blah From blah
Frome is a town in Somerset.

