/*
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2015 The PHP Group                                |
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
   +----------------------------------------------------------------------+
 */
/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/file.h"
#include "php_mailparse.h"
#include "ext/standard/info.h"
#include "main/php_output.h"
#include "php_open_temporary_file.h"

/* just in case the config check doesn't enable mbstring automatically */
#if !HAVE_MBSTRING
#error The mailparse extension requires the mbstring extension!
#endif

#define MAILPARSE_DECODE_NONE		0		/* include headers and leave section untouched */
#define MAILPARSE_DECODE_8BIT		1		/* decode body into 8-bit */
#define MAILPARSE_DECODE_NOHEADERS	2		/* don't include the headers */
#define MAILPARSE_DECODE_NOBODY		4		/* don't include the body */

#define MAILPARSE_EXTRACT_OUTPUT	0		/* extract to output buffer */
#define MAILPARSE_EXTRACT_STREAM	1		/* extract to a stream (caller supplies) */
#define MAILPARSE_EXTRACT_RETURN	2		/* return extracted data as a string */

static int extract_part(php_mimepart *part, int decode, php_stream *src, void *callbackdata,
		php_mimepart_extract_func_t callback TSRMLS_DC);
static int extract_callback_stream(php_mimepart *part, void *ptr, const char *p, size_t n TSRMLS_DC);
static int extract_callback_stdout(php_mimepart *part, void *ptr, const char *p, size_t n TSRMLS_DC);

static int get_structure_callback(php_mimepart *part, php_mimepart_enumerator *id, void *ptr TSRMLS_DC);
static int mailparse_get_part_data(php_mimepart *part, zval *return_value TSRMLS_DC);
static int mailparse_mimemessage_populate(php_mimepart *part, zval *object TSRMLS_DC);
static size_t mailparse_do_uudecode(php_stream *instream, php_stream *outstream TSRMLS_DC);

static int le_mime_part;


static zend_function_entry mimemessage_methods[] = {
	PHP_NAMED_FE(mimemessage,			PHP_FN(mailparse_mimemessage),					NULL)
	PHP_NAMED_FE(get_child,				PHP_FN(mailparse_mimemessage_get_child),		NULL)
	PHP_NAMED_FE(get_child_count,		PHP_FN(mailparse_mimemessage_get_child_count),	NULL)
	PHP_NAMED_FE(get_parent,			PHP_FN(mailparse_mimemessage_get_parent),		NULL)
	PHP_NAMED_FE(extract_headers,		PHP_FN(mailparse_mimemessage_extract_headers),	NULL)
	PHP_NAMED_FE(extract_body,			PHP_FN(mailparse_mimemessage_extract_body),		NULL)
	PHP_NAMED_FE(enum_uue,				PHP_FN(mailparse_mimemessage_enum_uue),			NULL)
	PHP_NAMED_FE(extract_uue,			PHP_FN(mailparse_mimemessage_extract_uue),		NULL)
	PHP_NAMED_FE(remove,				PHP_FN(mailparse_mimemessage_remove),			NULL)
	PHP_NAMED_FE(add_child,				PHP_FN(mailparse_mimemessage_add_child),		NULL)
	{NULL, NULL, NULL}
};

static zend_class_entry *mimemsg_class_entry;

zend_function_entry mailparse_functions[] = {
	PHP_FE(mailparse_msg_parse_file,			NULL)
	PHP_FE(mailparse_msg_get_part,				NULL)
	PHP_FE(mailparse_msg_get_structure,			NULL)
	PHP_FE(mailparse_msg_get_part_data,			NULL)
	PHP_FE(mailparse_msg_extract_part,			NULL)
	PHP_FE(mailparse_msg_extract_part_file,		NULL)
	PHP_FE(mailparse_msg_extract_whole_part_file,		NULL)

	PHP_FE(mailparse_msg_create,				NULL)
	PHP_FE(mailparse_msg_free,				NULL)
	PHP_FE(mailparse_msg_parse,				NULL)
	PHP_FE(mailparse_rfc822_parse_addresses,		NULL)
	PHP_FE(mailparse_determine_best_xfer_encoding, NULL)
	PHP_FE(mailparse_stream_encode,						NULL)
	PHP_FE(mailparse_uudecode_all,					NULL)

	PHP_FE(mailparse_test,	NULL)

	{NULL, NULL, NULL}
};

zend_module_entry mailparse_module_entry = {
	STANDARD_MODULE_HEADER,
	"mailparse",
	mailparse_functions,
	PHP_MINIT(mailparse),
	PHP_MSHUTDOWN(mailparse),
	PHP_RINIT(mailparse),
	PHP_RSHUTDOWN(mailparse),
	PHP_MINFO(mailparse),
	PHP_MAILPARSE_VERSION,
	STANDARD_MODULE_PROPERTIES
};

ZEND_DECLARE_MODULE_GLOBALS(mailparse)

#ifdef COMPILE_DL_MAILPARSE
ZEND_GET_MODULE(mailparse)
#endif

ZEND_RSRC_DTOR_FUNC(mimepart_dtor)
{
	php_mimepart *part = res->ptr;

	//TODO Sean-Der
	//if (part->parent == NULL && part->rsrc_id) {
	//	part->rsrc_id = 0;
	//	php_mimepart_free(part TSRMLS_CC);
	//}
}

PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("mailparse.def_charset", "us-ascii", PHP_INI_ALL, OnUpdateString, def_charset, zend_mailparse_globals, mailparse_globals)
PHP_INI_END()

#define mailparse_msg_name	"mailparse_mail_structure"

#define mailparse_fetch_mimepart_resource(rfcvar, zvalarg) rfcvar = (php_mimepart *)zend_fetch_resource(Z_RES_P(zvalarg), mailparse_msg_name, le_mime_part)

PHP_MAILPARSE_API int php_mailparse_le_mime_part(void)
{
	return le_mime_part;
}

PHP_MINIT_FUNCTION(mailparse)
{
	zend_class_entry mmce;

#ifdef ZTS
	zend_mailparse_globals *mailparse_globals;

	ts_allocate_id(&mailparse_globals_id, sizeof(zend_mailparse_globals), NULL, NULL);
	mailparse_globals = ts_resource(mailparse_globals_id);
#endif

	INIT_CLASS_ENTRY(mmce, "mimemessage", mimemessage_methods);
	mimemsg_class_entry = zend_register_internal_class(&mmce TSRMLS_CC);


	le_mime_part = zend_register_list_destructors_ex(mimepart_dtor, NULL, mailparse_msg_name, module_number);

	REGISTER_LONG_CONSTANT("MAILPARSE_EXTRACT_OUTPUT", MAILPARSE_EXTRACT_OUTPUT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MAILPARSE_EXTRACT_STREAM", MAILPARSE_EXTRACT_STREAM, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MAILPARSE_EXTRACT_RETURN", MAILPARSE_EXTRACT_RETURN, CONST_CS | CONST_PERSISTENT);

	REGISTER_INI_ENTRIES();

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(mailparse)
{
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}

/* {{{ ------------- MimeMessage methods */

static inline php_mimepart *mimemsg_get_object(zval *object TSRMLS_DC)
{
	zval *zpart;
	php_mimepart *part;
	int type;


	if (Z_TYPE_P(object) != IS_OBJECT) {
		return NULL;
	}

	if ((zpart = zend_hash_index_find(Z_OBJPROP_P(object), 0)) == NULL) {
		return NULL;
	}

	// TODO Sean-Der
	//part = zend_list_find(Z_LVAL_PP(zpart), &type);

	if (type != le_mime_part)
		return NULL;

	return part;
}

static int mailparse_mimemessage_populate(php_mimepart *part, zval *object TSRMLS_DC)
{
	zval tmp;

	mailparse_get_part_data(part, &tmp TSRMLS_CC);
	add_property_zval(object, "data", &tmp);
	Z_DELREF_P(&tmp);

	return SUCCESS;
}

static int mailparse_mimemessage_export(php_mimepart *part, zval *object TSRMLS_DC)
{
	zval zpart;

	//TODO Sean-Der
	//zend_list_addref(part->rsrc_id);

	//TODO Sean-Der
	//php_mimepart_to_zval(zpart, part);

	object_init_ex(object, mimemsg_class_entry);

	//TODO Sean-Der
	//Z_SET_ISREF_TO_P(object, 1);
	//Z_SET_REFCOUNT_P(object, 1);

	zend_hash_index_update(Z_OBJPROP_P(object), 0, &zpart);

	/* recurses for any of our child parts */
	mailparse_mimemessage_populate(part, object TSRMLS_CC);

	return SUCCESS;
}

PHP_FUNCTION(mailparse_mimemessage)
{
	zval *object = getThis();
	php_mimepart *part;
	zval zpart;
	char *mode;
	int mode_len;
	zval *source = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz!", &mode, &mode_len, &source) == FAILURE)
		RETURN_FALSE;

	/* prepare the mime part for this object */
	part = php_mimepart_alloc(TSRMLS_C);
	//TODO Sean-Der
	//php_mimepart_to_zval(zpart, part);

	zend_hash_index_update(Z_OBJPROP_P(object), 0, &zpart);

	/* now check the args */

	if (strcmp(mode, "new") == 0)
		RETURN_TRUE;

	if (source == NULL)
		RETURN_FALSE;

	if (strcmp(mode, "var") == 0 && Z_TYPE_P(source) == IS_STRING) {
		/* source is the actual message */
		part->source.kind = mpSTRING;

		part->source.zval = *source;
		zval_copy_ctor(&part->source.zval);
		Z_SET_REFCOUNT_P(&part->source.zval, 1);
		convert_to_string_ex(&part->source.zval);
	}

	if (strcmp(mode, "file") == 0) {
		/* source is the name of a file */
		php_stream *srcstream;

		part->source.kind = mpSTREAM;
		convert_to_string_ex(source);
		srcstream = php_stream_open_wrapper(Z_STRVAL_P(source), "rb", REPORT_ERRORS, NULL);

		if (srcstream == NULL) {
			RETURN_FALSE;
		}

		php_stream_to_zval(srcstream, &part->source.zval);
	}
	if (strcmp(mode, "stream") == 0) {
		part->source.kind = mpSTREAM;

		part->source.zval = *source;
		zval_copy_ctor(&part->source.zval);
		Z_SET_REFCOUNT_P(&part->source.zval, 1);
		convert_to_string_ex(&part->source.zval);
	}

	/* parse the data from the source */
	if (part->source.kind == mpSTRING) {
		php_mimepart_parse(part, Z_STRVAL_P(&part->source.zval), Z_STRLEN_P(&part->source.zval) TSRMLS_CC);
	} else if (part->source.kind == mpSTREAM) {
		php_stream *srcstream;
		char buf[1024];

		php_stream_from_zval(srcstream, &part->source.zval);

		php_stream_rewind(srcstream);
		while(!php_stream_eof(srcstream)) {
			size_t n = php_stream_read(srcstream, buf, sizeof(buf));
			if (n > 0)
				php_mimepart_parse(part, buf, n TSRMLS_CC);
		}
	}

	mailparse_mimemessage_populate(part, object TSRMLS_CC);
}

PHP_FUNCTION(mailparse_mimemessage_remove)
{
	php_mimepart *part;

	part = mimemsg_get_object(getThis() TSRMLS_CC);
	if (part == NULL)
		RETURN_FALSE;

	php_mimepart_remove_from_parent(part TSRMLS_CC);
}

PHP_FUNCTION(mailparse_mimemessage_add_child)
{
	php_mimepart *part;

	part = mimemsg_get_object(getThis() TSRMLS_CC);
	if (part == NULL)
		RETURN_FALSE;

	php_mimepart_remove_from_parent(part TSRMLS_CC);
}


PHP_FUNCTION(mailparse_mimemessage_get_child_count)
{
	php_mimepart *part;

	part = mimemsg_get_object(getThis() TSRMLS_CC);
	if (part == NULL)
		RETURN_FALSE;

	RETURN_LONG(zend_hash_num_elements(&part->children));
}

PHP_FUNCTION(mailparse_mimemessage_get_parent)
{
	php_mimepart *part;

	part = mimemsg_get_object(getThis() TSRMLS_CC);

	if (part && part->parent) {
		mailparse_mimemessage_export(part->parent, return_value TSRMLS_CC);
	} else {
		RETURN_NULL();
	}
}

PHP_FUNCTION(mailparse_mimemessage_get_child)
{
	php_mimepart *part, *foundpart;
	zval *item_to_find_int;
	zend_string *item_to_find_str;

	part = mimemsg_get_object(getThis() TSRMLS_CC);

	if (part == NULL)
		RETURN_NULL();

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|l", &item_to_find_str, &item_to_find_int) == FAILURE) {
		RETURN_NULL();
	}

	if (item_to_find_str) {
		foundpart = php_mimepart_find_by_name(part, item_to_find_str->val TSRMLS_CC);
	} else {
		foundpart = php_mimepart_find_child_by_position(part, Z_LVAL_P(item_to_find_int) TSRMLS_CC);
	}

	if (!foundpart)	{
		RETURN_NULL();
	}

	mailparse_mimemessage_export(foundpart, return_value TSRMLS_CC);
}

static void mailparse_mimemessage_extract(int flags, INTERNAL_FUNCTION_PARAMETERS)
{
	php_mimepart *part;
	zval *zarg = NULL;
	php_stream *srcstream, *deststream = NULL;
	long mode = MAILPARSE_EXTRACT_OUTPUT;
	php_mimepart_extract_func_t callback = extract_callback_stdout;
	void *callback_data = NULL;

	part = mimemsg_get_object(getThis() TSRMLS_CC);

	RETVAL_NULL();

	if (part == NULL)
		return;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|lz!", &mode, &zarg))
		return;

	switch(mode) {
		case MAILPARSE_EXTRACT_STREAM:
			if (zarg == NULL) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Parameter 2 must be a stream");
				return;
			}

			php_stream_from_zval(deststream, zarg);
			break;
		case MAILPARSE_EXTRACT_RETURN:
			deststream = php_stream_memory_create(TEMP_STREAM_DEFAULT);
			break;
	}


	if (part->source.kind == mpSTRING)
		srcstream = php_stream_memory_open(TEMP_STREAM_READONLY, Z_STRVAL_P(&part->source.zval), Z_STRLEN_P(&part->source.zval));
	else
		php_stream_from_zval(srcstream, &part->source.zval);

	if (srcstream == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "MimeMessage object is missing a source stream!");
		goto cleanup;
	}

	if (deststream != NULL) {
		callback_data = deststream;
		callback = extract_callback_stream;
	}

	if (SUCCESS == extract_part(part, flags, srcstream, callback_data, callback TSRMLS_CC)) {

		if (mode == MAILPARSE_EXTRACT_RETURN) {
			size_t len;
			char *buf;

			buf = php_stream_memory_get_buffer(deststream, &len);
			RETVAL_STRINGL(buf, len);
		} else {
			RETVAL_TRUE;
		}

	}

cleanup:

	if (part->source.kind == mpSTRING && srcstream)
		php_stream_close(srcstream);
	if (mode == MAILPARSE_EXTRACT_RETURN && deststream)
		php_stream_close(deststream);

}

PHP_FUNCTION(mailparse_mimemessage_extract_headers)
{
	mailparse_mimemessage_extract(MAILPARSE_DECODE_NOBODY, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_FUNCTION(mailparse_mimemessage_extract_body)
{
	mailparse_mimemessage_extract(MAILPARSE_DECODE_NOHEADERS | MAILPARSE_DECODE_8BIT, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_FUNCTION(mailparse_mimemessage_extract_uue)
{
	php_mimepart *part;
	zval *zarg = NULL;
	php_stream *srcstream, *deststream = NULL;
	long mode = MAILPARSE_EXTRACT_OUTPUT;
	long index = 0; /* which uue to extract */
	off_t end;
	off_t start_pos;
	char buffer[4096];
	int nparts = 0;

	part = mimemsg_get_object(getThis() TSRMLS_CC);

	RETVAL_NULL();

	if (part == NULL)
		return;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|lz!", &index, &mode, &zarg))
		return;

	switch(mode) {
		case MAILPARSE_EXTRACT_STREAM:
			if (zarg == NULL) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Parameter 2 must be a stream");
				return;
			}

			php_stream_from_zval(deststream, zarg);
			break;
		case MAILPARSE_EXTRACT_RETURN:
			deststream = php_stream_memory_create(TEMP_STREAM_DEFAULT);
			break;
		case MAILPARSE_EXTRACT_OUTPUT:
			deststream = php_stream_open_wrapper("php://output", "wb", 0, NULL);
			break;
	}

	if (part->source.kind == mpSTRING)
		srcstream = php_stream_memory_open(TEMP_STREAM_READONLY, Z_STRVAL_P(&part->source.zval), Z_STRLEN_P(&part->source.zval));
	else
		php_stream_from_zval(srcstream, &part->source.zval);

	if (srcstream == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "MimeMessage object is missing a source stream!");
		goto cleanup;
	}

	/* position stream at start of the body for this part */

	start_pos = part->bodystart;
	end = part->parent ? part->bodyend : part->endpos;
	php_stream_seek(srcstream, start_pos, SEEK_SET);

	while(!php_stream_eof(srcstream) && php_stream_gets(srcstream, buffer, sizeof(buffer))) {
		/* Look for the "begin " sequence that identifies a uuencoded file */
		if (strncmp(buffer, "begin ", 6) == 0) {
			char *origfilename;
			int len;

			/* parse out the file name.
			 * The next 4 bytes are an octal number for perms; ignore it */
			origfilename = &buffer[10];
			/* NUL terminate the filename */
			len = strlen(origfilename);
			while(isspace(origfilename[len-1]))
				origfilename[--len] = '\0';

			/* make the return an array */
			if (nparts == index) {
				mailparse_do_uudecode(srcstream, deststream TSRMLS_CC);
				if (mode == MAILPARSE_EXTRACT_RETURN) {
					size_t len;
					char *buf;

					buf = php_stream_memory_get_buffer(deststream, &len);
					RETVAL_STRINGL(buf, len);
				} else {
					RETVAL_TRUE;
				}

				break;
			} else {
				/* skip that part */
				mailparse_do_uudecode(srcstream, NULL TSRMLS_CC);
			}
		} else {
			if (php_stream_tell(srcstream) >= end)
				break;
		}
	}

cleanup:

	if (part->source.kind == mpSTRING && srcstream)
		php_stream_close(srcstream);
	if (mode != MAILPARSE_EXTRACT_STREAM && deststream)
		php_stream_close(deststream);


}

PHP_FUNCTION(mailparse_mimemessage_enum_uue)
{
	php_stream *instream;
	php_mimepart *part;
	off_t end;
	off_t start_pos, curr_pos;
	size_t file_size;
	char buffer[4096];
	int nparts = 0;
	zval item;

	part = mimemsg_get_object(getThis() TSRMLS_CC);

	RETVAL_FALSE;

	if (part == NULL)
		return;

	if (part->source.kind == mpSTRING)
		instream = php_stream_memory_open(TEMP_STREAM_READONLY, Z_STRVAL_P(&part->source.zval), Z_STRLEN_P(&part->source.zval));
	else
		php_stream_from_zval(instream, &part->source.zval);

	if (instream == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "MimeMessage object is missing a source stream!");
		goto cleanup;
	}

	/* position stream at start of the body for this part */

	start_pos = part->bodystart;
	end = part->parent ? part->bodyend : part->endpos;
	php_stream_seek(instream, start_pos, SEEK_SET);

	while(!php_stream_eof(instream) && php_stream_gets(instream, buffer, sizeof(buffer))) {
		/* Look for the "begin " sequence that identifies a uuencoded file */
		if (strncmp(buffer, "begin ", 6) == 0) {
			char *origfilename;
			int len;

			/* parse out the file name.
			 * The next 4 bytes are an octal number for perms; ignore it */
			origfilename = &buffer[10];
			/* NUL terminate the filename */
			len = strlen(origfilename);
			while(isspace(origfilename[len-1]))
				origfilename[--len] = '\0';

			/* make the return an array */
			if (nparts == 0) {
				array_init(return_value);
			}

			/* add an item */
			array_init(&item);
			add_assoc_string(&item, "filename", origfilename);
			add_assoc_long(&item, "start-pos", php_stream_tell(instream));

			/* decode it and remember the file size */
			file_size = mailparse_do_uudecode(instream, NULL TSRMLS_CC);
			add_assoc_long(&item, "filesize", file_size);

			curr_pos = php_stream_tell(instream);

			if (curr_pos > end) {
				/* we somehow overran the message boundary; the message itself is
				 * probably bogus, so lets cancel this item */
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "uue attachment overran part boundary; this should not happen, message is probably malformed");
				zval_ptr_dtor(&item);
				break;
			}

			add_assoc_long(&item, "end-pos", curr_pos);
			add_next_index_zval(return_value, &item);
			nparts++;

		} else {
			if (php_stream_tell(instream) >= end)
				break;
		}
	}
cleanup:
	if (part->source.kind == mpSTRING && instream)
		php_stream_close(instream);

}

/* --- END ---------- MimeMessage methods }}} */

PHP_MINFO_FUNCTION(mailparse)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "mailparse support", "enabled");
	php_info_print_table_row(2, "Extension Version", PHP_MAILPARSE_VERSION);
	php_info_print_table_row(2, "Revision", "$Revision$");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}


PHP_RINIT_FUNCTION(mailparse)
{
	return SUCCESS;
}


PHP_RSHUTDOWN_FUNCTION(mailparse)
{
	return SUCCESS;
}

#define UUDEC(c)	(char)(((c)-' ')&077)
#define UU_NEXT(v)	if (line[x] == '\0' || line[x] == '\r' || line[x] == '\n') break; v = line[x++]; v = UUDEC(v)
static size_t mailparse_do_uudecode(php_stream *instream, php_stream *outstream TSRMLS_DC)
{
	int x, A, B, C, D, n;
	size_t file_size = 0;
	char line[128];

	if (outstream) {
		/* write to outstream */
		while(!php_stream_eof(instream))	{
			if (!php_stream_gets(instream, line, sizeof(line))) {
				break;
			}
			x = 0;

			UU_NEXT(n);

			while(n != 0)	{
				UU_NEXT(A);
				UU_NEXT(B);
				UU_NEXT(C);
				UU_NEXT(D);

				if (n-- > 0) {
					file_size++;
					php_stream_putc(outstream, (A << 2) | (B >> 4));
				}

				if (n-- > 0) {
					file_size++;
					php_stream_putc(outstream, (B << 4) | (C >> 2));
				}

				if (n-- > 0) {
					file_size++;
					php_stream_putc(outstream, (C << 6) | D);
				}
			}
		}
	} else {
		/* skip (and measure) the data, but discard it.
		 * This is separated from the version above to speed it up by a few cycles */

		while(!php_stream_eof(instream))	{

			if (!php_stream_gets(instream, line, sizeof(line))) {
				break;
			}
			x = 0;

			UU_NEXT(n);

			while(line[x] && n != 0) {
				UU_NEXT(A);
				UU_NEXT(B);
				UU_NEXT(C);
				UU_NEXT(D);

				if (n-- > 0) {
					file_size++;
				}

				if (n-- > 0) {
					file_size++;
				}

				if (n-- > 0) {
					file_size++;
				}
			}
		}
	}
	return file_size;
}

/* php_stream_fopen_temporary_file auto unlink the file on close
 * this will keep the file
 */
static php_stream *_mailparse_create_stream(zend_string **path) {
	int fd;

	fd = php_open_temporary_fd(NULL, "mailparse", path);
	if (fd != -1)	{
		return php_stream_fopen_from_fd(fd, "r+b", NULL);
	}
	return NULL;
}

/* {{{ proto array mailparse_uudecode_all(resource fp)
   Scans the data from fp and extract each embedded uuencoded file. Returns an array listing filename information */
PHP_FUNCTION(mailparse_uudecode_all)
{
	zval *file, item;
	char *buffer = NULL;
	zend_string *outpath;
	int nparts = 0;
	php_stream *instream, *outstream = NULL, *partstream = NULL;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &file))
		return;

	php_stream_from_zval(instream, file);

	outstream = _mailparse_create_stream(&outpath);
	if (outstream == NULL)	{
		zend_error(E_WARNING, "%s(): unable to open temp file", get_active_function_name(TSRMLS_C));
		RETURN_FALSE;
	}

	php_stream_rewind(instream);

	buffer = emalloc(4096);
	while(php_stream_gets(instream, buffer, 4096)) {
		/* Look for the "begin " sequence that identifies a uuencoded file */
		if (strncmp(buffer, "begin ", 6) == 0) {
			char * origfilename;
			int len;

			/* parse out the file name.
			 * The next 4 bytes are an octal number for perms; ignore it */
			origfilename = &buffer[10];
			/* NUL terminate the filename */
			len = strlen(origfilename);
			while(isspace(origfilename[len-1]))
				origfilename[--len] = '\0';

			/* make the return an array */
			if (nparts == 0) {
				array_init(return_value);
				/* create an initial item representing the file with all uuencoded parts
				 * removed */
				array_init(&item);
				add_assoc_string(&item, "filename", outpath->val);
				add_next_index_zval(return_value, &item);
			}

			/* add an item */
			array_init(&item);
			add_assoc_string(&item, "origfilename", origfilename);

			/* create a temp file for the data */
			partstream = _mailparse_create_stream(&outpath);
			if (partstream)	{
				nparts++;
				add_assoc_string(&item, "filename", outpath->val);
				add_next_index_zval(return_value, &item);

				/* decode it */
				mailparse_do_uudecode(instream, partstream TSRMLS_CC);
				php_stream_close(partstream);
			}
		} else {
			/* write to the output file */
			php_stream_write_string(outstream, buffer);
		}
	}
	php_stream_close(outstream);
	php_stream_rewind(instream);
	efree(buffer);

	if (nparts == 0) {
		/* delete temporary file */
		unlink(outpath->val);
		efree(outpath);
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto array mailparse_rfc822_parse_addresses(string addresses)
   Parse addresses and returns a hash containing that data */
PHP_FUNCTION(mailparse_rfc822_parse_addresses)
{
	char *addresses;
	int addresses_len;
	php_rfc822_tokenized_t *toks = NULL;
	php_rfc822_addresses_t *addrs = NULL;
	int i;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &addresses, &addresses_len) == FAILURE) {
		RETURN_FALSE;
	}
	toks = php_mailparse_rfc822_tokenize((const char*)addresses, 1 TSRMLS_CC);
	addrs = php_rfc822_parse_address_tokens(toks);

	array_init(return_value);

	for (i = 0; i < addrs->naddrs; i++) {
		zval item;

		array_init(&item);

		if (addrs->addrs[i].name)
			add_assoc_string(&item, "display", addrs->addrs[i].name);
		if (addrs->addrs[i].address)
			add_assoc_string(&item, "address", addrs->addrs[i].address);
		add_assoc_bool(&item, "is_group", addrs->addrs[i].is_group);

		zend_hash_next_index_insert(Z_ARRVAL_P(return_value), &item);
	}

	php_rfc822_free_addresses(addrs);
	php_rfc822_tokenize_free(toks);
}
/* }}} */

/* {{{ proto string mailparse_determine_best_xfer_encoding(resource fp)
   Figures out the best way of encoding the content read from the file pointer fp, which must be seek-able */
PHP_FUNCTION(mailparse_determine_best_xfer_encoding)
{
	zval *file;
	int longline = 0;
	int linelen = 0;
	int c;
	enum mbfl_no_encoding bestenc = mbfl_no_encoding_7bit;
	php_stream *stream;
	char * name;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &file) == FAILURE)	{
		RETURN_FALSE;
	}

	php_stream_from_zval(stream, file);

	php_stream_rewind(stream);
	while(!php_stream_eof(stream))	{
		c = php_stream_getc(stream);
		if (c == EOF)
			break;
		if (c > 0x80)
			bestenc = mbfl_no_encoding_8bit;
		else if (c == 0)	{
			bestenc = mbfl_no_encoding_base64;
			longline = 0;
			break;
		}
		if (c == '\n')
			linelen = 0;
		else if (++linelen > 200)
			longline = 1;
	}
	if (longline)
		bestenc = mbfl_no_encoding_qprint;
	php_stream_rewind(stream);

	name = (char *)mbfl_no2preferred_mime_name(bestenc);
	if (name)
	{
		RETVAL_STRING(name);
	}
	else
	{
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ proto boolean mailparse_stream_encode(resource sourcefp, resource destfp, string encoding)
   Streams data from source file pointer, apply encoding and write to destfp */

static int mailparse_stream_output(int c, void *stream MAILPARSE_MBSTRING_TSRMLS_DC)
{
	char buf = c;
	MAILPARSE_MBSTRING_TSRMLS_FETCH_IF_BRAIN_DEAD();

	return php_stream_write((php_stream*)stream, &buf, 1);
}
static int mailparse_stream_flush(void *stream MAILPARSE_MBSTRING_TSRMLS_DC)
{
	MAILPARSE_MBSTRING_TSRMLS_FETCH_IF_BRAIN_DEAD();
	return php_stream_flush((php_stream*)stream);
}

PHP_FUNCTION(mailparse_stream_encode)
{
	zval *srcfile, *destfile;
	zend_string *encod;
	php_stream *srcstream, *deststream;
	char *buf;
	size_t len;
	size_t bufsize = 2048;
	enum mbfl_no_encoding enc;
	mbfl_convert_filter *conv = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rrS", &srcfile, &destfile, &encod) == FAILURE)	{
		RETURN_FALSE;
	}

	if (Z_TYPE_P(srcfile) == IS_RESOURCE && Z_LVAL_P(srcfile) == 0)	{
		RETURN_FALSE;
	}
	if (Z_TYPE_P(destfile) == IS_RESOURCE && Z_LVAL_P(destfile) == 0)	{
		RETURN_FALSE;
	}

	php_stream_from_zval(srcstream, srcfile);
	php_stream_from_zval(deststream, destfile);

	enc = mbfl_name2no_encoding(encod->val);
	if (enc == mbfl_no_encoding_invalid)	{
		zend_error(E_WARNING, "%s(): unknown encoding \"%s\"",
				get_active_function_name(TSRMLS_C),
				encod->val
				);
		RETURN_FALSE;
	}

	buf = emalloc(bufsize);
	RETVAL_TRUE;

	conv = mbfl_convert_filter_new(mbfl_no_encoding_8bit,
			enc,
			mailparse_stream_output,
			mailparse_stream_flush,
			deststream
			MAILPARSE_MBSTRING_TSRMLS_CC
			);

	if (enc == mbfl_no_encoding_qprint) {
		/* If the qp encoded section is going to be digitally signed,
		 * it is a good idea to make sure that lines that begin "From "
		 * have the letter F encoded, so that MTAs do not stick a > character
		 * in front of it and invalidate the content/signature */
		while(!php_stream_eof(srcstream))	{
			if (NULL != php_stream_gets(srcstream, buf, bufsize)) {
				size_t i;

				len = strlen(buf);

				if (strncmp(buf, "From ", 5) == 0) {
					mbfl_convert_filter_flush(conv MAILPARSE_MBSTRING_TSRMLS_CC);
					php_stream_write(deststream, "=46rom ", 7);
					i = 5;
				} else {
					i = 0;
				}

				for (; i<len; i++)
					mbfl_convert_filter_feed(buf[i], conv MAILPARSE_MBSTRING_TSRMLS_CC);
			}
		}

	} else {
		while(!php_stream_eof(srcstream))	{
			len = php_stream_read(srcstream, buf, bufsize);
			if (len > 0)
			{
				size_t i;
				for (i=0; i<len; i++)
					mbfl_convert_filter_feed(buf[i], conv MAILPARSE_MBSTRING_TSRMLS_CC);
			}
		}
	}

	mbfl_convert_filter_flush(conv MAILPARSE_MBSTRING_TSRMLS_CC);
	mbfl_convert_filter_delete(conv MAILPARSE_MBSTRING_TSRMLS_CC);
	efree(buf);
}
/* }}} */

/* {{{ proto void mailparse_msg_parse(resource mimepart, string data)
   Incrementally parse data into buffer */
PHP_FUNCTION(mailparse_msg_parse)
{
	zend_string *data;
	zval *arg;
	php_mimepart *part;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rS", &arg, &data) == FAILURE)	{
		RETURN_FALSE;
	}

	mailparse_fetch_mimepart_resource(part, arg);

	if (FAILURE == php_mimepart_parse(part, data->val, data->len TSRMLS_CC)) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto resource mailparse_msg_parse_file(string filename)
   Parse file and return a resource representing the structure */
PHP_FUNCTION(mailparse_msg_parse_file)
{
	char *filename;
	int filename_len;
	php_mimepart *part;
	char *filebuf;
	php_stream *stream;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &filename, &filename_len) == FAILURE)	{
		RETURN_FALSE;
	}

	/* open file and read it in */
	stream = php_stream_open_wrapper(filename, "rb", REPORT_ERRORS, NULL);
	if (stream == NULL)	{
		RETURN_FALSE;
	}

	filebuf = emalloc(MAILPARSE_BUFSIZ);

	part = php_mimepart_alloc(TSRMLS_C);
	// TODO Sean-Der
	//php_mimepart_to_zval(return_value, part);

	while(!php_stream_eof(stream))	{
		int got = php_stream_read(stream, filebuf, MAILPARSE_BUFSIZ);
		if (got > 0)	{
			if (FAILURE == php_mimepart_parse(part, filebuf, got TSRMLS_CC)) {
				/* We have to destroy the already allocated part, if we not return it */
				php_mimepart_free(part TSRMLS_CC);
				RETVAL_FALSE;
				break;
			}
		}
	}
	php_stream_close(stream);
	efree(filebuf);
}
/* }}} */

/* {{{ proto void mailparse_msg_free(resource mimepart)
   Frees a handle allocated by mailparse_msg_create
*/
PHP_FUNCTION(mailparse_msg_free)
{
	zval *arg;
	php_mimepart *part;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &arg) == FAILURE)	{
		RETURN_FALSE;
	}

	mailparse_fetch_mimepart_resource(part, arg);
	/* zend_list_delete(Z_LVAL_P(arg)); */
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int mailparse_msg_create(void)
   Returns a handle that can be used to parse a message */
PHP_FUNCTION(mailparse_msg_create)
{
	php_mimepart *part = php_mimepart_alloc(TSRMLS_C);

	RETURN_RES(part->rsrc);
}
/* }}} */

static int get_structure_callback(php_mimepart *part, php_mimepart_enumerator *id, void *ptr TSRMLS_DC)
{
	zval *return_value = (zval *)ptr;
	char intbuf[16];
	char *buf;
	int buf_size;
	int len, i = 0;

	buf_size = 1024;
	buf = emalloc(buf_size);
	while(id && i < buf_size)	{
		sprintf(intbuf, "%d", id->id);
		len = strlen(intbuf);
		if (len > (buf_size-i))	{
			/* too many sections: bail */
			zend_error(E_WARNING, "%s(): too many nested sections in message", get_active_function_name(TSRMLS_C));
			return FAILURE;
		}
		if ((i + len + 1) >= buf_size) {
			buf_size = buf_size << 1;
			buf = erealloc(buf, buf_size);
			if (!buf) {
				zend_error(E_ERROR, "The structure buffer has been exceeded (%d).  Please try decreasing the nesting depth of messages and report this to the developers.", buf_size);
			}
		}
		sprintf(&buf[i], "%s%c", intbuf, id->next ? '.' : '\0');
		i += len + (id->next ? 1 : 0);
		id = id->next;
	}
	add_next_index_string(return_value, buf);
	return SUCCESS;
}

/* {{{ proto array mailparse_msg_get_structure(resource mimepart)
   Returns an array of mime section names in the supplied message */
PHP_FUNCTION(mailparse_msg_get_structure)
{
	zval *arg;
	php_mimepart *part;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &arg) == FAILURE)	{
		RETURN_FALSE;
	}

	mailparse_fetch_mimepart_resource(part, arg);

	if (array_init(return_value) == FAILURE)	{
		RETURN_FALSE;
	}
	php_mimepart_enum_parts(part, &get_structure_callback, return_value TSRMLS_CC);
}
/* }}} */

/* callback for decoding using a "userdefined" php function */
static int extract_callback_user_func(php_mimepart *part, zval *userfunc, const char *p, size_t n TSRMLS_DC)
{
	zval retval, arg;

	ZVAL_FALSE(&retval);

	ZVAL_STRINGL(&arg, (char*)p, (int)n);

	/* TODO: use zend_is_callable */

	if (call_user_function(EG(function_table), NULL, userfunc, &retval, 1, &arg TSRMLS_CC) == FAILURE)
		zend_error(E_WARNING, "%s(): unable to call user function", get_active_function_name(TSRMLS_C));

	zval_ptr_dtor(&retval);
	zval_ptr_dtor(&arg);

	return 0;
}

/* callback for decoding to the current output buffer */
static int extract_callback_stdout(php_mimepart *part, void *ptr, const char *p, size_t n TSRMLS_DC)
{
	ZEND_WRITE(p, n);
	return 0;
}

/* callback for decoding to a stream */
static int extract_callback_stream(php_mimepart *part, void *ptr, const char *p, size_t n TSRMLS_DC)
{
	php_stream_write((php_stream*)ptr, p, n);
	return 0;
}

#define MAILPARSE_DECODE_NONE		0		/* include headers and leave section untouched */
#define MAILPARSE_DECODE_8BIT		1		/* decode body into 8-bit */
#define MAILPARSE_DECODE_NOHEADERS	2		/* don't include the headers */
#define MAILPARSE_DECODE_NOBODY		4		/* don't include the body */

static int extract_part(php_mimepart *part, int decode, php_stream *src, void *callbackdata,
		php_mimepart_extract_func_t callback TSRMLS_DC)
{
	off_t end;
	off_t start_pos;
	char *filebuf = NULL;
	int ret = FAILURE;

	/* figure out where the message part starts/ends */
	start_pos = decode & MAILPARSE_DECODE_NOHEADERS ? part->bodystart : part->startpos;

	if (decode & MAILPARSE_DECODE_NOBODY)
		end = part->bodystart;
	else
		end = part->parent ? part->bodyend : part->endpos;

	php_mimepart_decoder_prepare(part, decode & MAILPARSE_DECODE_8BIT, callback, callbackdata TSRMLS_CC);

	if (php_stream_seek(src, start_pos, SEEK_SET) == -1) {
		zend_error(E_WARNING, "%s(): unable to seek to section start", get_active_function_name(TSRMLS_C));
		goto cleanup;
	}

	filebuf = emalloc(MAILPARSE_BUFSIZ);

	while (start_pos < end)
	{
		size_t n = MAILPARSE_BUFSIZ - 1;

		if ((off_t)n > end - start_pos)
			n = end - start_pos;

		n = php_stream_read(src, filebuf, n);

		if (n == 0)
		{
			zend_error(E_WARNING, "%s(): error reading from file at offset %ld", get_active_function_name(TSRMLS_C), start_pos);
			goto cleanup;
		}

		filebuf[n] = '\0';

		php_mimepart_decoder_feed(part, filebuf, n TSRMLS_CC);

		start_pos += n;
	}
	ret = SUCCESS;

cleanup:
	php_mimepart_decoder_finish(part TSRMLS_CC);

	if (filebuf)
		efree(filebuf);

	return ret;
}

static void mailparse_do_extract(INTERNAL_FUNCTION_PARAMETERS, int decode, int isfile)
{
	zval *zpart, *filename, *callbackfunc = NULL;
	php_mimepart *part;
	php_stream *srcstream = NULL, *deststream = NULL;
	php_mimepart_extract_func_t cbfunc = NULL;
	void *cbdata = NULL;
	int close_src_stream = 0;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rz|z", &zpart, &filename, &callbackfunc)) {
		RETURN_FALSE;
	}

	mailparse_fetch_mimepart_resource(part, zpart);

	/* filename can be a filename or a stream */
	if (Z_TYPE_P(filename) == IS_RESOURCE) {
		php_stream_from_zval(srcstream, filename);
	} else if (isfile) {
		convert_to_string_ex(filename);
		srcstream = php_stream_open_wrapper(Z_STRVAL_P(filename), "rb", REPORT_ERRORS, NULL);
		close_src_stream = 1;
	} else {
		/* filename is the actual data */
		srcstream = php_stream_memory_open(TEMP_STREAM_READONLY, Z_STRVAL_P(filename), Z_STRLEN_P(filename));
		close_src_stream = 1;
	}

	if (srcstream == NULL) {
		RETURN_FALSE;
	}

	if (callbackfunc != NULL) {
		if (Z_TYPE_P(callbackfunc) == IS_NULL) {
			cbfunc = extract_callback_stream;
			cbdata = deststream = php_stream_memory_create(TEMP_STREAM_DEFAULT);
		} else if (Z_TYPE_P(callbackfunc) == IS_RESOURCE) {
			//TODO Sean-Der
			//php_stream_from_zval(deststream, &callbackfunc);
			cbfunc = extract_callback_stream;
			cbdata = deststream;
			deststream = NULL; /* don't free this one */
		} else {
			if (Z_TYPE_P(callbackfunc) != IS_ARRAY)
				convert_to_string_ex(callbackfunc);
			cbfunc = (php_mimepart_extract_func_t)&extract_callback_user_func;
			cbdata = callbackfunc;
		}
	} else {
		cbfunc = extract_callback_stdout;
		cbdata = NULL;
	}

	RETVAL_FALSE;

	if (SUCCESS == extract_part(part, decode, srcstream, cbdata, cbfunc TSRMLS_CC)) {

		if (deststream != NULL) {
			/* return it's contents as a string */
			char *membuf = NULL;
			size_t memlen = 0;
			membuf = php_stream_memory_get_buffer(deststream, &memlen);
			RETVAL_STRINGL(membuf, memlen);

		} else {
			RETVAL_TRUE;
		}
	}

	if (deststream)
		php_stream_close(deststream);
	if (close_src_stream && srcstream)
		php_stream_close(srcstream);
}

/* {{{ proto void mailparse_msg_extract_part(resource mimepart, string msgbody[, string callbackfunc])
   Extracts/decodes a message section.  If callbackfunc is not specified, the contents will be sent to "stdout" */
PHP_FUNCTION(mailparse_msg_extract_part)
{
	mailparse_do_extract(INTERNAL_FUNCTION_PARAM_PASSTHRU, MAILPARSE_DECODE_8BIT | MAILPARSE_DECODE_NOHEADERS, 0);
}
/* }}} */

/* {{{ proto string mailparse_msg_extract_whole_part_file(resource mimepart, string filename [, string callbackfunc])
   Extracts a message section including headers without decoding the transfer encoding */
PHP_FUNCTION(mailparse_msg_extract_whole_part_file)
{
	mailparse_do_extract(INTERNAL_FUNCTION_PARAM_PASSTHRU, MAILPARSE_DECODE_NONE, 1);
}
/* }}} */

/* {{{ proto string mailparse_msg_extract_part_file(resource mimepart, string filename [, string callbackfunc])
   Extracts/decodes a message section, decoding the transfer encoding */
PHP_FUNCTION(mailparse_msg_extract_part_file)
{
	mailparse_do_extract(INTERNAL_FUNCTION_PARAM_PASSTHRU, MAILPARSE_DECODE_8BIT | MAILPARSE_DECODE_NOHEADERS, 1);
}
/* }}} */

static void add_attr_header_to_zval(char *valuelabel, char *attrprefix, zval *return_value,
		struct php_mimeheader_with_attributes *attr TSRMLS_DC)
{
	HashPosition pos;
	zval *val;
	char *newkey;
	ulong num_index;
	zend_string *str_key;

	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(&attr->attributes), &pos);
	while ((val = zend_hash_get_current_data_ex(Z_ARRVAL_P(&attr->attributes), &pos)) != NULL) {

		zend_hash_get_current_key_ex(Z_ARRVAL_P(&attr->attributes), &str_key, &num_index, &pos);

    if (str_key) {
      spprintf(&newkey, 0, "%s%s", attrprefix, str_key->val);
    } else {
      spprintf(&newkey, 0, "%s%lu", attrprefix, num_index);
    }
    add_assoc_string(return_value, newkey, Z_STRVAL_P(val));
    efree(newkey);

		zend_hash_move_forward_ex(Z_ARRVAL_P(&attr->attributes), &pos);
	}

	/* do this last so that a bogus set of headers like this:
	 * Content-Type: multipart/related;
	 *    boundary="----=_NextPart_00_0017_01C091F4.1B5EF6B0";
	 *    type="text/html"
	 *
	 * doesn't overwrite content-type with the type="text/html"
	 * value.
	 * */
	add_assoc_string(return_value, valuelabel, attr->value);
}

static void add_header_reference_to_zval(char *headerkey, zval *return_value, zval *headers TSRMLS_DC)
{
	zval *headerval, *newhdr;
	zend_string *hash_key;

	hash_key = zend_string_init(headerkey, strlen(headerkey), 0);
	if ((headerval = zend_hash_find(Z_ARRVAL_P(headers), hash_key)) != NULL) {
		*newhdr = *headerval;
		// TODO Sean-Der
		//Z_SET_REFCOUNT_P(newhdr, 1);
		zval_copy_ctor(newhdr);
		add_assoc_zval(return_value, headerkey, newhdr);
	}
	zend_string_release(hash_key);
}

static int mailparse_get_part_data(php_mimepart *part, zval *return_value TSRMLS_DC)
{
	zval *headers, *tmpval;
	zend_string *hash_key;
	off_t startpos, endpos, bodystart;
	int nlines, nbodylines;

	array_init(return_value);

	/* get headers for this section */
	headers = &part->headerhash;
	zval_copy_ctor(headers);

	add_assoc_zval(return_value, "headers", headers);

	php_mimepart_get_offsets(part, &startpos, &endpos, &bodystart, &nlines, &nbodylines);

	add_assoc_long(return_value, "starting-pos",		startpos);
	add_assoc_long(return_value, "starting-pos-body",	bodystart);
	add_assoc_long(return_value, "ending-pos",			endpos);
	add_assoc_long(return_value, "ending-pos-body",		part->bodyend);
	add_assoc_long(return_value, "line-count",			nlines);
	add_assoc_long(return_value, "body-line-count",		nbodylines);

	if (part->charset)
		add_assoc_string(return_value, "charset", part->charset);
	else
		add_assoc_string(return_value, "charset", MAILPARSEG(def_charset));

	if (part->content_transfer_encoding)
		add_assoc_string(return_value, "transfer-encoding", part->content_transfer_encoding);
	else
		add_assoc_string(return_value, "transfer-encoding", "8bit");

	if (part->content_type)
		add_attr_header_to_zval("content-type", "content-", return_value, part->content_type TSRMLS_CC);
	else
		add_assoc_string(return_value, "content-type", "text/plain; (error)");

	if (part->content_disposition)
		add_attr_header_to_zval("content-disposition", "disposition-", return_value, part->content_disposition TSRMLS_CC);

	if (part->content_location)
		add_assoc_string(return_value, "content-location", part->content_location);

	if (part->content_base)
		add_assoc_string(return_value, "content-base", part->content_base);
	else
		add_assoc_string(return_value, "content-base", "/");

	if (part->boundary)
		add_assoc_string(return_value, "content-boundary", part->boundary);

	/* extract the address part of the content-id only */
	hash_key = zend_string_init("content-id", sizeof("content-id") - 1, 0);
	if ((tmpval = zend_hash_find(Z_ARRVAL_P(headers), hash_key)) != NULL) {
		php_rfc822_tokenized_t *toks;
		php_rfc822_addresses_t *addrs;

		toks = php_mailparse_rfc822_tokenize(Z_STRVAL_P(tmpval), 1 TSRMLS_CC);
		addrs = php_rfc822_parse_address_tokens(toks);
		if (addrs->naddrs > 0)
			add_assoc_string(return_value, "content-id", addrs->addrs[0].address);
		php_rfc822_free_addresses(addrs);
		php_rfc822_tokenize_free(toks);
	}

	add_header_reference_to_zval("content-description", return_value, headers TSRMLS_CC);
	add_header_reference_to_zval("content-language", return_value, headers TSRMLS_CC);
	add_header_reference_to_zval("content-md5", return_value, headers TSRMLS_CC);

	return SUCCESS;
}

/* {{{ proto array mailparse_msg_get_part_data(resource mimepart)
   Returns an associative array of info about the message */
PHP_FUNCTION(mailparse_msg_get_part_data)
{
	zval *arg;
	php_mimepart *part;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &arg) == FAILURE)	{
		RETURN_FALSE;
	}

	mailparse_fetch_mimepart_resource(part, arg);

	mailparse_get_part_data(part, return_value TSRMLS_CC);
}
/* }}} */

/* {{{ proto int mailparse_msg_get_part(resource mimepart, string mimesection)
   Returns a handle on a given section in a mimemessage */
PHP_FUNCTION(mailparse_msg_get_part)
{
	zval *arg;
	php_mimepart *part, *foundpart;
	zend_string *mimesection;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rS", &arg, &mimesection) == FAILURE)	{
		RETURN_FALSE;
	}

	mailparse_fetch_mimepart_resource(part, arg);

	foundpart = php_mimepart_find_by_name(part, mimesection->val TSRMLS_CC);

	if (!foundpart)	{
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "cannot find section %s in message", mimesection);
		RETURN_FALSE;
	}
	//TODO Sean-Der
	//zend_list_addref(foundpart->rsrc_id);
	//php_mimepart_to_zval(return_value, foundpart);
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
