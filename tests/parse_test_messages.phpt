--TEST--
Parse messages in testdata dir
--SKIPIF--
<?php 
/* vim600: sw=4 ts=4 fdm=marker syn=php
*/
if (!extension_loaded("mailparse") || !extension_loaded("zlib")) print "skip"; ?>
--FILE--
<?php
error_reporting(E_ALL ^ E_NOTICE);

$define_expect = isset($argv[1]) && $argv[1] == "define_expect";
$force_test = isset($argv[1]) && !$define_expect ? $argv[1] : null;
$testdir = dirname(__FILE__) . "/testdata";

$dir = opendir($testdir) or die("unable to open test dir!");
$messages = array();

while (($f = readdir($dir)) !== false) {
	if ($f == "CVS" || $f == "." || $f == "..")
		continue;
		
	list($name, $suffix) = explode(".", $f, 2);
	
	switch($suffix) {
		case "txt":
		case "txt.gz":
			$messages[$name]["testfile"] = $f;
			break;
		case "exp":
			$messages[$name]["expectfile"] = $f;
			break;
	}
}

if ($force_test !== null) {
	$messages = array($force_test => $messages[$force_test]);
}

if (function_exists("version_compare") && version_compare(phpversion(), "4.3", "ge")) {
	$wrapper = "compress.zlib://";
} else {
	/* this section is here because it is useful to compare to the
	 * original implementaion of mailparse for PHP 4.2 */
	$wrapper = "zlib:";
	
	function file_get_contents($filename)
	{
		$fp = fopen($filename, "rb");
		$data = fread($fp, filesize($filename));
		fclose($fp);
		return $data;
	}
}

function diff_strings($left, $right)
{
	if (is_executable("/usr/bin/diff")) {
		$lf = tempnam("/tmp", "mpt");
		$rf = tempnam("/tmp", "mpt");

		$ok = false;

		$fp = fopen($lf, "wb");
		if ($fp) {
			fwrite($fp, $left);
			fclose($fp);

			$fp = fopen($rf, "wb");
			if ($fp) {
				fwrite($fp, $right);
				fclose($fp);

				$ok = true;
			}
		}

		if ($ok) {
			passthru("/usr/bin/diff -u $lf $rf");
		}

		unlink($lf);
		unlink($rf);

		if ($ok)
			return;
	}

	
	$left = explode("\n", $left);
	$right = explode("\n", $right);

	$n = max(count($left), count($right));

	$difflines = array();

	$runstart = null;
	$runend = null;

	for ($i = 0; $i < $n; $i++) {
		if ($left[$i] != $right[$i]) {
			if ($runstart === null) {
				$runstart = $i;
				$runend = $i;
			} else {
				/* part of the run */
				$runend = $i;
			}
		} else {
			if ($runstart !== null) {
				$difflines[] = array($runstart, $runend);
				$runstart = null;
				$runend = null;
			}
		}
	}
	if ($runstart !== null)
		$difflines[] = array($runstart, $runend);
	
	$lastprint = null;
	foreach ($difflines as $run) {
		list($start, $end) = $run;

		$startline = $start - 3;
		if ($startline < 0)
			$startline = 0;
		$endline = $end;

		if ($lastprint === null) {
			echo "@@ Line: " . ($startline+1) . "\n";
		} else if ($startline <= $lastprint) {
			$startline = $lastprint+1;
		}

		if ($startline > $endline)
			continue;

		/* starting context */
		for ($i = $startline; $i < $start; $i++) {
			echo " " . $left[$i] . "\n";
			$lastprint = $i;
		}

		/* diff run */
		for ($i = $start; $i <= $end; $i++) {
			echo "-" . $left[$i] . "\n";
		}
		for ($i = $start; $i <= $end; $i++) {
			echo "+" . $right[$i] . "\n";
		}
		$lastprint = $i;
	}
	
}

$skip_keys = array("headers", "ending-pos-body");

foreach ($messages as $name => $msgdata) {
	$testname = $testdir . "/" . $msgdata["testfile"];
	$expectname = $testdir . "/" . $msgdata["expectfile"];

	$use_wrapper = substr($testname, -3) == ".gz" ? $wrapper : "";
	$use_wrapper = $wrapper;
	$fp = fopen("$use_wrapper$testname", "rb") or die("failed to open the file!");

	$mime = mailparse_msg_create();
	$size = 0;
	while (!feof($fp)) {
		$data = fread($fp, 1024);
		//var_dump($data);
		if ($data !== false) {
			mailparse_msg_parse($mime, $data);
			$size += strlen($data);
		}
	}
	fclose($fp);
	//var_dump($size);

	$struct = mailparse_msg_get_structure($mime);

	ob_start();
	echo "Message: $name\n";
	foreach($struct as $partname) {
		$depth = count(explode(".", $partname)) - 1;
		$indent = str_repeat("  ", $depth * 2);
		$subpart = mailparse_msg_get_part($mime, $partname);
		if (!$subpart) {
			var_dump($partname);
			echo "\n";
			var_dump($struct);
			break;
		}

		$data = mailparse_msg_get_part_data($subpart);
		echo "\n{$indent}Part $partname\n";
		ksort($data);
		foreach ($data as $key => $value) {
			if (in_array($key, $skip_keys))
				continue;
			echo "$indent$key => ";
			var_dump($value);
		}
	}
	$output = ob_get_contents();

	if ($define_expect) {
		$fp = fopen($expectname, "wb");
		fwrite($fp, $output);
		fclose($fp);
	} else {

		$expect = file_get_contents($expectname);

		if ($output != $expect) {
			ob_end_flush();
			diff_strings($expect, $output);
			die("FAIL!");
		}
	}

	ob_end_clean();
}

echo "All messages parsed OK!\n";
?>
--EXPECT--
All messages parsed OK!
