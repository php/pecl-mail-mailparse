
$(top_srcdir)/ext/mailparse/php_mailparse_rfc822.c: $(top_srcdir)/ext/mailparse/php_mailparse_rfc822.re
	re2c -b $(top_srcdir)/ext/mailparse/php_mailparse_rfc822.re > $@

