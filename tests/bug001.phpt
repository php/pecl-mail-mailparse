--TEST--
mailparse_msg_free causes double free segfault
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) print "skip"; ?>
--FILE--
<?php
$path =  dirname(__FILE__) . '/testdata/m0001.txt';

$resource = mailparse_msg_parse_file($path);
$stream = fopen($path, 'r');

$structure = mailparse_msg_get_structure($resource);
$parts = array();
foreach ($structure as $part_id) {
            $part = mailparse_msg_get_part($resource, $part_id);
            $parts[$part_id] = mailparse_msg_get_part_data($part);
}

if (is_resource($stream)) {
    fclose($stream);
}
if (is_resource($resource)) {
    mailparse_msg_free($resource);
}
print 'No Segfault!' . PHP_EOL;

--EXPECT--
No Segfault!
