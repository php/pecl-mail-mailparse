
php_mailparse_rfc822.c: php_mailparse_rfc822.re
	re2c -b php_mailparse_rfc822.re > $@

