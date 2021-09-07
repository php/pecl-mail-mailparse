--TEST--
Bug #81422 (Potential double-free in mailparse_uudecode_all())
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--FILE--
<?php
$data = <<<'EOD'
begin 644 foo
`
end

begin 644 bar
`
end

EOD;
$stream = fopen('php://memory', 'w+');
fwrite($stream, $data);
rewind($stream);
$parsed = mailparse_uudecode_all($stream);
var_dump(count($parsed));
?>
--EXPECT--
int(3)
