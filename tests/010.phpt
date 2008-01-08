--TEST--
Multiple headers not parsed into arra bug #6862
--SKIPIF--
<?php 
/* vim600: sw=4 ts=4 fdm=marker syn=php
*/
if (!extension_loaded("mailparse")) print "skip"; ?>
--FILE--
<?php
$msg = <<<EOD
Received: from mail pickup service by hotmail.com with Microsoft
SMTPSVC;
Sat, 18 Feb 2006 22:58:14 -0800
Received: from 66.178.40.49 by BAY116-DAV8.phx.gbl with DAV;
Sun, 19 Feb 2006 06:58:13 +0000

test
EOD;

$mail = mailparse_msg_create();
mailparse_msg_parse($mail,$msg);
$struct = mailparse_msg_get_structure($mail);

foreach($struct as $st) {
    $section = mailparse_msg_get_part($mail, $st);
    $info = mailparse_msg_get_part_data($section);
    var_dump($info);
}
?>
--EXPECT--
array(11) {
  ["headers"]=>
  array(1) {
    ["received"]=>
    array(2) {
      [0]=>
      string(54) "from mail pickup service by hotmail.com with Microsoft"
      [1]=>
      string(50) "from 66.178.40.49 by BAY116-DAV8.phx.gbl with DAV;"
    }
  }
  ["starting-pos"]=>
  int(0)
  ["starting-pos-body"]=>
  int(200)
  ["ending-pos"]=>
  int(200)
  ["ending-pos-body"]=>
  int(200)
  ["line-count"]=>
  int(6)
  ["body-line-count"]=>
  int(0)
  ["charset"]=>
  string(8) "us-ascii"
  ["transfer-encoding"]=>
  string(4) "8bit"
  ["content-type"]=>
  string(10) "text/plain"
  ["content-base"]=>
  string(1) "/"
}
