/*
   +----------------------------------------------------------------------+
   | PHP Version 4                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2002 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Wez Furlong <wez@thebrainroom.com>                           |
   +----------------------------------------------------------------------+
 */
/* $Id$ */

#ifndef php_mailparse_mime_h
#define php_mailparse_mime_h

#include "ext/standard/php_smart_str.h"

typedef struct _php_mimepart php_mimepart;

struct php_mimeheader_with_attributes {
	char *value;
	zval *attributes;
};

PHPAPI char *php_mimepart_attribute_get(struct php_mimeheader_with_attributes *attr, char *attrname);

typedef int (*php_mimepart_extract_func_t)(php_mimepart *part, void *context, const char *buf, size_t n TSRMLS_DC);

struct _php_mimepart {
	php_mimepart *parent;
	long rsrc_id;		/* for auto-cleanup */
	int part_index;		/* sequence number of this part */
	HashTable children;	/* child parts */
	
	off_t startpos, endpos;		/* offsets of this part in the message */
	off_t bodystart, bodyend;	/* offsets of the body content of this part */
	size_t nlines, nbodylines;	/* number of lines in section/body */

	char *mime_version;
	char *content_transfer_encoding;
	char *content_location;
	char *content_base;
	char *boundary;
	char *charset;
	
	struct php_mimeheader_with_attributes *content_type, *content_disposition;
	
	zval *headerhash; /* a record of all the headers */
	
	/* these are used during part extraction */
	php_mimepart_extract_func_t extract_func;
	mbfl_convert_filter *extract_filter;
	void *extract_context;

	/* these are used during parsing */
	struct {
		int in_header:1;
		int is_dummy:1;
		int completed:1;

		smart_str workbuf;
		smart_str headerbuf;
	} parsedata;
};

PHPAPI php_mimepart *php_mimepart_alloc(void);
PHPAPI void php_mimepart_free(php_mimepart *part);
PHPAPI int php_mimepart_parse(php_mimepart *part, const char *buf, size_t bufsize TSRMLS_DC);

PHPAPI void php_mimepart_decoder_prepare(php_mimepart *part, int do_decode, php_mimepart_extract_func_t decoder, void *ptr TSRMLS_DC);
PHPAPI void php_mimepart_decoder_finish(php_mimepart *part TSRMLS_DC);
PHPAPI int php_mimepart_decoder_feed(php_mimepart *part, const char *buf, size_t bufsize TSRMLS_DC);

#define php_mimepart_to_zval(zval, part)	ZVAL_RESOURCE(zval, part->rsrc_id)

typedef struct _php_mimepart_enumerator php_mimepart_enumerator;
struct _php_mimepart_enumerator {
	php_mimepart_enumerator *next;
	int id;
};
typedef int (*mimepart_enumerator_func)(php_mimepart *part, php_mimepart_enumerator *enumerator, void *ptr TSRMLS_DC);

PHPAPI void php_mimepart_enum_parts(php_mimepart *part, mimepart_enumerator_func callback, void *ptr TSRMLS_DC);
PHPAPI php_mimepart *php_mimepart_find_by_name(php_mimepart *parent, const char *name TSRMLS_DC);

#endif

