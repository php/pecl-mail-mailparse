# mailparse library for PHP

Mailparse is an extension for parsing and working with email messages.

It can deal with rfc822 and rfc2045 (MIME) compliant messages.

Mailparse is stream based, which means that it does not keep in-memory
copies of the files it processes - so it is very resource efficient
when dealing with large messages.

Version 2.1.6 is for PHP 5

## OO Syntax

```php
<?php
$file = "/path/to/rfc822/compliant/message";
// parse the message in $file.
// The file MUST remain in existence until you are finished using
// the object, as mailparse does not cache the file in memory.
// You can also use two alternative syntaxes:
//
// Read the message from a variable:
//   $msg = new MimeMessage("var", $message);
//
// Read the message from a stream (from fopen).
// The stream MUST be seekable, or things will not work correctly.
// Also, you MUST NOT fclose the stream until you have finished
// using the message object (or any child objects it returns).
//   $msg = new MimeMessage("stream", $fp);
//
$msg = new MimeMessage("file", $file);

// Process the message.
display_part_info("message", $msg);

// Little function to display things
function display_part_info($caption, &$msgpart)
{
	echo "Message part: $caption\n";

	// The data member corresponds to the information
	// available from the mailparse_msg_get_part_data function.
	// You can access a particular header like this:
	//   $subject = $msgpart->data["headers"]["subject"];
	var_dump($msgpart->data);

	echo "The headers are:\n";
	// Display the headers (in raw format) to the browser output.
	// You can also use:
	//   $msgpart->extract_headers(MAILPARSE_EXTRACT_STREAM, $fp);
	//     to write the headers to the supplied stream at it's current
	//     position.
	//
	//   $var = $msgpart->extract_headers(MAILPARSE_EXTRACT_RETURN);
	//     to return the headers in a variable.
	$msgpart->extract_headers(MAILPARSE_EXTRACT_OUTPUT);

	// Display the body if this part is intended to be displayed:
	$n = $msgpart->get_child_count();

	if ($n == 0) {
		// Return the body as a string (the MAILPARSE_EXTRACT parameter
		// acts just as it does in extract_headers method.
		$body = $msgpart->extract_body(MAILPARSE_EXTRACT_RETURN);
		echo htmlentities($body);

		// This function tells you about any uuencoded attachments
		// that are present in this part.
		$uue = $msgpart->enum_uue();
		if ($uue !== false) {
			var_dump($uue);
			foreach($uue as $index => $data) {
				// $data => array("filename" => "original filename",
				//                "filesize" => "size of extracted file",
				//               );

				printf("UUE[%d] %s (%d bytes)\n",
					$index, $data["filename"],
					$data["filesize"]);

				// Display the extracted part to the output.
				$msgpart->extract_uue($index, MAILPARSE_EXTRACT_OUTPUT);

			}
		}

	} else {
		// Recurse and show children of that part
		for ($i = 0; $i < $n; $i++) {
			$part =& $msgpart->get_child($i);
			display_part_info("$caption child $i", $part);
		}
	}
}

```


The rest of this document may be out of date! Take a look at the [mailparse section of the online manual](http://php.net/manual/en/book.mailparse.php) for more hints about this stuff.

$mime = mailparse_rfc2045_parse_file($file);
$ostruct = mailparse_rfc2045_getstructure($mime);
foreach($ostruct as $st)	{
	$section = mailparse_rfc2045_find($mime, $st);
	$struct[$st] = mailparse_rfc2045_getinfo($section);
}
var_dump($struct);
?>
array mailparse_rfc822_parse_addresses(string addresses)
	parses an rfc822 compliant recipient list, such as that found in To: From:
	headers.  Returns a indexed array of assoc. arrays for each recipient:
	array(0 => array("display" => "Wez Furlong", "address" => "wez@php.net"))

resource mailparse_rfc2045_create()
	Create a mime mail resource

boolean mailparse_rfc2045_parse(resource mimemail, string data)
	incrementally parse data into the supplied mime mail resource.
	Concept: you can stream portions of a file at a time, rather than read
	and parse the whole thing.


resource mailparse_rfc2045_parse_file(string $filename)
	Parse a file and return a $mime resource.
	The file is opened and streamed through the parser.
	This is the optimal way of parsing a mail file that
	you have on disk.


array mailparse_rfc2045_getstructure(resource mimemail)
	returns an array containing a list of message parts in the form:
	array("1", "1.1", "1.2")

resource mailparse_rfc2045_find(resource mimemail, string partname)
	returns an mime mail resource representing the named section

array mailparse_rfc2045_getinfo(resource mimemail)
	returns an array containing the bounds, content type and headers of the
  	section.

mailparse_rfc2045_extract_file(resource mimemail, string filename[, string
		callbackfunc])
	Extracts/decodes a message section from the supplied filename.
	If no callback func is supplied, it outputs the results into the current
	output buffer, otherwise it calls the callback with a string parameter
	containing the text.
	The contents of the section will be decoded according to their transfer
	encoding - base64, quoted-printable and uuencoded text are supported.

All operations are done incrementally; streaming the input and output so that
memory usage is on the whole lower than something like procmail or doing this
stuff in PHP space.  The aim is that it stays this way to handle large
quantities of email.


TODO:
=====

. Add support for binhex encoding?
. Extracting a message part without decoding the transfer encoding so that
	eg: pgp-signatures can be verified.

. Work the other way around - build up a rfc2045 compliant message file from
	simple structure information and filenames/variables.

vim:tw=78
vim600:syn=php:tw=78
