--TEST--
Multiple UUE attachments not recognized
--SKIPIF--
<?php 
/* vim600: sw=4 ts=4 fdm=marker syn=php
*/
if (!extension_loaded("mailparse")) print "skip"; ?>
--FILE--
<?php
error_reporting(E_ALL);
$msg = new MimeMessage("file", dirname(__FILE__) . "/testdata/oeuue");
$n = $msg->get_child_count();
$uue = $msg->enum_uue();
var_dump($n);
var_dump($uue);
?>
--EXPECT--
int(0)
array(3) {
  [0]=>
  array(4) {
    ["filename"]=>
    string(11) "README1.dat"
    ["start-pos"]=>
    int(654)
    ["filesize"]=>
    int(88)
    ["end-pos"]=>
    int(785)
  }
  [1]=>
  array(4) {
    ["filename"]=>
    string(11) "README2.dat"
    ["start-pos"]=>
    int(808)
    ["filesize"]=>
    int(88)
    ["end-pos"]=>
    int(939)
  }
  [2]=>
  array(4) {
    ["filename"]=>
    string(11) "README3.dat"
    ["start-pos"]=>
    int(962)
    ["filesize"]=>
    int(88)
    ["end-pos"]=>
    int(1093)
  }
}
