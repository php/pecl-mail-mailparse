/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/3_01.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Wez Furlong <wez@thebrainroom.com>                           |
   | Credit also given to Double Precision Inc. who wrote the code that   |
   | the support routines for this extension were based upon.             |
   +----------------------------------------------------------------------+
 */

#ifndef PHP_MAILPARSE_H
#define PHP_MAILPARSE_H

extern zend_module_entry mailparse_module_entry;
#define phpext_mailparse_ptr &mailparse_module_entry

#define PHP_MAILPARSE_VERSION "3.1.3"

#ifdef PHP_WIN32
#define PHP_MAILPARSE_API __declspec(dllexport)
#else
#define PHP_MAILPARSE_API
#endif

PHP_MINIT_FUNCTION(mailparse);
PHP_MSHUTDOWN_FUNCTION(mailparse);
PHP_RINIT_FUNCTION(mailparse);
PHP_RSHUTDOWN_FUNCTION(mailparse);
PHP_MINFO_FUNCTION(mailparse);

PHP_FUNCTION(mailparse_msg_parse_file);
PHP_FUNCTION(mailparse_msg_get_part);
PHP_FUNCTION(mailparse_msg_get_structure);
PHP_FUNCTION(mailparse_msg_get_part_data);
PHP_FUNCTION(mailparse_msg_extract_part);
PHP_FUNCTION(mailparse_msg_extract_part_file);
PHP_FUNCTION(mailparse_msg_extract_whole_part_file);

PHP_FUNCTION(mailparse_msg_create);
PHP_FUNCTION(mailparse_msg_free);
PHP_FUNCTION(mailparse_msg_parse);
PHP_FUNCTION(mailparse_msg_parse_file);

PHP_FUNCTION(mailparse_msg_find);
PHP_FUNCTION(mailparse_msg_getstructure);
PHP_FUNCTION(mailparse_msg_getinfo);
PHP_FUNCTION(mailparse_msg_extract);
PHP_FUNCTION(mailparse_msg_extract_file);
PHP_FUNCTION(mailparse_rfc822_parse_addresses);
PHP_FUNCTION(mailparse_determine_best_xfer_encoding);
PHP_FUNCTION(mailparse_stream_encode);
PHP_FUNCTION(mailparse_uudecode_all);

PHP_FUNCTION(mailparse_test);

PHP_MAILPARSE_API int php_mailparse_le_mime_part(void);
PHP_MAILPARSE_API char* php_mailparse_msg_name(void);

/* mimemessage object */
PHP_METHOD(mimemessage, __construct);
PHP_METHOD(mimemessage, get_child);
PHP_METHOD(mimemessage, get_child_count);
PHP_METHOD(mimemessage, get_parent);
PHP_METHOD(mimemessage, extract_headers);
PHP_METHOD(mimemessage, extract_body);
PHP_METHOD(mimemessage, enum_uue);
PHP_METHOD(mimemessage, extract_uue);
PHP_METHOD(mimemessage, remove);
PHP_METHOD(mimemessage, add_child);

# include "ext/mbstring/libmbfl/mbfl/mbfilter.h"

#include "php_mailparse_rfc822.h"
#include "php_mailparse_mime.h"

#define MAILPARSE_BUFSIZ		4096
ZEND_BEGIN_MODULE_GLOBALS(mailparse)
    char * def_charset;	/* default charset for use in (re)writing mail */
ZEND_END_MODULE_GLOBALS(mailparse);

extern ZEND_DECLARE_MODULE_GLOBALS(mailparse);


#ifdef ZTS
#define MAILPARSEG(v) TSRMG(mailparse_globals_id, zend_mailparse_globals *, v)
#else
#define MAILPARSEG(v) (mailparse_globals.v)
#endif

#endif


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: sw=4 ts=4
 */
