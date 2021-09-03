--TEST--
Bug #75825 (mailparse_uudecode_all doesn't parse multiple files)
--SKIPIF--
<?php
if (!extension_loaded("mailparse")) die("skip mailparse extension not available");
?>
--FILE--
<?php
$data = <<<'EOD'
begin 644 .profile
M(R!^+RYP<F]F:6QE.B!E>&5C=71E9"!B>2!T:&4@8V]M;6%N9"!I;G1E<G!R
M971E<B!F;W(@;&]G:6X@<VAE;&QS+@HC(%1H:7,@9FEL92!I<R!N;W0@<F5A
M9"!B>2!B87-H*#$I+"!I9B!^+RYB87-H7W!R;V9I;&4@;W(@?B\N8F%S:%]L
M;V=I;@HC(&5X:7-T<RX*(R!S964@+W5S<B]S:&%R92]D;V,O8F%S:"]E>&%M
M<&QE<R]S=&%R='5P+69I;&5S(&9O<B!E>&%M<&QE<RX*(R!T:&4@9FEL97,@
M87)E(&QO8V%T960@:6X@=&AE(&)A<V@M9&]C('!A8VMA9V4N"@HC('1H92!D
M969A=6QT('5M87-K(&ES('-E="!I;B`O971C+W!R;V9I;&4[(&9O<B!S971T
M:6YG('1H92!U;6%S:PHC(&9O<B!S<V@@;&]G:6YS+"!I;G-T86QL(&%N9"!C
M;VYF:6=U<F4@=&AE(&QI8G!A;2UU;6%S:R!P86-K86=E+@HC=6UA<VL@,#(R
M"@HC(&EF(')U;FYI;F<@8F%S:`II9B!;("UN("(D0D%32%]615)324].(B!=
M.R!T:&5N"B`@("`C(&EN8VQU9&4@+F)A<VAR8R!I9B!I="!E>&ES=',*("`@
M(&EF(%L@+68@(B1(3TU%+RYB87-H<F,B(%T[('1H96X*"2X@(B1(3TU%+RYB
M87-H<F,B"B`@("!F:0IF:0H*(R!S970@4$%42"!S;R!I="!I;F-L=61E<R!U
M<V5R)W,@<')I=F%T92!B:6X@:68@:70@97AI<W1S"FEF(%L@+60@(B1(3TU%
M+V)I;B(@72`[('1H96X*("`@(%!!5$@](B1(3TU%+V)I;CHD4$%42"(*9FD*
`
end
begin 644 .bash_logout
M(R!^+RYB87-H7VQO9V]U=#H@97AE8W5T960@8GD@8F%S:"@Q*2!W:&5N(&QO
M9VEN('-H96QL(&5X:71S+@H*(R!W:&5N(&QE879I;F<@=&AE(&-O;G-O;&4@
M8VQE87(@=&AE('-C<F5E;B!T;R!I;F-R96%S92!P<FEV86-Y"@II9B!;("(D
M4TA,5DPB(#T@,2!=.R!T:&5N"B`@("!;("UX("]U<W(O8FEN+V-L96%R7V-O
H;G-O;&4@72`F)B`O=7-R+V)I;B]C;&5A<E]C;VYS;VQE("UQ"F9I"@``
`
end
begin 644 .selected_editor
`
end

EOD;
$h = fopen('php://memory', 'w+');
$h || die("Failed to open string!\n");
fwrite($h, $data);
rewind($h);
$parsed = mailparse_uudecode_all($h);
fclose($h);
var_dump($parsed);
?>
--EXPECTF--
array(4) {
  [0]=>
  array(1) {
    ["filename"]=>
    string(%d) "%s"
  }
  [1]=>
  array(2) {
    ["origfilename"]=>
    string(8) ".profile"
    ["filename"]=>
    string(%d) "%s"
  }
  [2]=>
  array(2) {
    ["origfilename"]=>
    string(12) ".bash_logout"
    ["filename"]=>
    string(%d) "%s"
  }
  [3]=>
  array(2) {
    ["origfilename"]=>
    string(16) ".selected_editor"
    ["filename"]=>
    string(%d) "%s"
  }
}
