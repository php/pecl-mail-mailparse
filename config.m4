PHP_ARG_ENABLE(mailparse, whether to enable mailparse support,
[  --enable-mailparse      Enable mailparse support.])

if test "$PHP_MAILPARSE" != "no"; then
  if test "$ext_shared" != "yes" && test "$enable_mbstring" != "yes"; then
	  AC_MSG_WARN(Activating mbstring)
	  enable_mbstring=yes
  fi
  PHP_NEW_EXTENSION(mailparse, 	mailparse.c php_mailparse_mime.c php_mailparse_rfc822.c, $ext_shared)
  PHP_ADD_EXTENSION_DEP(mailparse, mbstring, true)
  PHP_ADD_MAKEFILE_FRAGMENT
fi

AC_MSG_CHECKING(for installed php mbstring extension)
php_ext_dir=$(php-config --extension-dir)
required_ext=$php_ext_dir/mbstring.so

if test -f "$required_ext"; then
	CFLAGS="$CFLAGS -DHAVE_MBSTRING"
	AC_MSG_RESULT(yes)
else
	AC_MSG_ERROR(no)
fi
