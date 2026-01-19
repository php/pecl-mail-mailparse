PHP_ARG_ENABLE(mailparse, whether to enable mailparse support,
[  --enable-mailparse      Enable mailparse support.])

if test "$PHP_MAILPARSE" != "no"; then
  PHP_NEW_EXTENSION(mailparse, mailparse.c php_mailparse_mime.c php_mailparse_rfc822.c mailparse_encoding.c, $ext_shared)
  PHP_ADD_MAKEFILE_FRAGMENT
fi
