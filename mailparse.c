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
   +----------------------------------------------------------------------+
 */

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

#include "arginfo.h"

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
		php_mimepart_extract_func_t callback);
static int extract_callback_stream(php_mimepart *part, void *ptr, const char *p, size_t n);
static int extract_callback_stdout(php_mimepart *part, void *ptr, const char *p, size_t n);

static int get_structure_callback(php_mimepart *part, php_mimepart_enumerator *id, void *ptr);
static int mailparse_get_part_data(php_mimepart *part, zval *return_value);
static int mailparse_mimemessage_populate(php_mimepart *part, zval *object);
static size_t mailparse_do_uudecode(php_stream *instream, php_stream *outstream);

static int le_mime_part;

static zend_function_entry mimemessage_methods[] = {
	PHP_ME(mimemessage, __construct,     arginfo_mailparse_mimemessage_construct,       ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(mimemessage, get_child,       arginfo_mailparse_mimemessage_get_child,       ZEND_ACC_PUBLIC)
	PHP_ME(mimemessage, get_child_count, arginfo_mailparse_mimemessage_get_child_count, ZEND_ACC_PUBLIC)
	PHP_ME(mimemessage, get_parent,      arginfo_mailparse_mimemessage_get_parent,      ZEND_ACC_PUBLIC)
	PHP_ME(mimemessage, extract_headers, arginfo_mailparse_mimemessage_extract_headers, ZEND_ACC_PUBLIC)
	PHP_ME(mimemessage, extract_body,    arginfo_mailparse_mimemessage_extract_body,    ZEND_ACC_PUBLIC)
	PHP_ME(mimemessage, enum_uue,        arginfo_mailparse_mimemessage_enum_uue,        ZEND_ACC_PUBLIC)
	PHP_ME(mimemessage, extract_uue,     arginfo_mailparse_mimemessage_extract_uue,     ZEND_ACC_PUBLIC)
	PHP_ME(mimemessage, remove,          arginfo_mailparse_mimemessage_remove,          ZEND_ACC_PUBLIC)
	PHP_ME(mimemessage, add_child,       arginfo_mailparse_mimemessage_add_child,       ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};

static zend_class_entry *mimemsg_class_entry;

zend_function_entry mailparse_functions[] = {
	PHP_FE(mailparse_msg_parse_file,				arginfo_mailparse_msg_parse_file)
	PHP_FE(mailparse_msg_get_part,					arginfo_mailparse_msg_get_part)
	PHP_FE(mailparse_msg_get_structure,				arginfo_mailparse_msg_get_structure)
	PHP_FE(mailparse_msg_get_part_data,				arginfo_mailparse_msg_get_part_data)
	PHP_FE(mailparse_msg_extract_part,				arginfo_mailparse_msg_extract_part)
	PHP_FE(mailparse_msg_extract_part_file,			arginfo_mailparse_msg_extract_part_file)
	PHP_FE(mailparse_msg_extract_whole_part_file,	arginfo_mailparse_msg_extract_whole_part_file)

	PHP_FE(mailparse_msg_create,					arginfo_mailparse_msg_create)
	PHP_FE(mailparse_msg_free,						arginfo_mailparse_msg_free)
	PHP_FE(mailparse_msg_parse,						arginfo_mailparse_msg_parse)
	PHP_FE(mailparse_rfc822_parse_addresses,		arginfo_mailparse_rfc822_parse_addresses)
	PHP_FE(mailparse_determine_best_xfer_encoding, 	arginfo_mailparse_determine_best_xfer_encoding)
	PHP_FE(mailparse_stream_encode,					arginfo_mailparse_stream_encode)
	PHP_FE(mailparse_uudecode_all,					arginfo_mailparse_uudecode_all)

	PHP_FE(mailparse_test,							arginfo_mailparse_test)

	PHP_FE_END
};

static const zend_module_dep mailparse_deps[] = {
	ZEND_MOD_REQUIRED("mbstring")
	ZEND_MOD_END
};

zend_module_entry mailparse_module_entry = {
	STANDARD_MODULE_HEADER_EX,
	NULL,
	mailparse_deps,
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

	if (part->parent == NULL) {
		php_mimepart_free(part);
	}
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

PHP_MAILPARSE_API char* php_mailparse_msg_name(void)
{
	return mailparse_msg_name;
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
	mimemsg_class_entry = zend_register_internal_class(&mmce);


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

static inline php_mimepart *mimemsg_get_object(zval *object)
{
	zval *zpart;
	php_mimepart *part;

	if (Z_TYPE_P(object) != IS_OBJECT) {
		return NULL;
	}

	if ((zpart = zend_hash_index_find(Z_OBJPROP_P(object), 0)) == NULL) {
		return NULL;
	}

	if ((mailparse_fetch_mimepart_resource(part, zpart)) == NULL) {
		return NULL;
	}

	return part;
}

static int mailparse_mimemessage_populate(php_mimepart *part, zval *object)
{
	zval tmp;

	mailparse_get_part_data(part, &tmp);
	add_property_zval(object, "data", &tmp);
	Z_DELREF_P(&tmp);

	return SUCCESS;
}

static int mailparse_mimemessage_export(php_mimepart *part, zval *object)
{
	zval zpart;

	part->rsrc->gc.refcount++;

	php_mimepart_to_zval(&zpart, part->rsrc);

	object_init_ex(object, mimemsg_class_entry);

	zend_hash_index_update(Z_OBJPROP_P(object), 0, &zpart);

	/* recurses for any of our child parts */
	mailparse_mimemessage_populate(part, object);

	return SUCCESS;
}

PHP_METHOD(mimemessage, __construct)
{
	zval *object = getThis();
	php_mimepart *part;
	zval zpart;
	zend_string *mode;
	zval *source = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz!", &mode, &source) == FAILURE)
		RETURN_FALSE;

	/* prepare the mime part for this object */
	part = php_mimepart_alloc();
	php_mimepart_to_zval(&zpart, part->rsrc);

	zend_hash_index_update(Z_OBJPROP_P(object), 0, &zpart);

	/* now check the args */

	if (zend_string_equals_literal(mode, "new"))
		RETURN_TRUE;

	if (source == NULL)
		RETURN_FALSE;

	if (zend_string_equals_literal(mode, "var") && Z_TYPE_P(source) == IS_STRING) {
		/* source is the actual message */
		part->source.kind = mpSTRING;

		ZVAL_DUP(&part->source.zval, source);
		convert_to_string_ex(&part->source.zval);
	}

	if (zend_string_equals_literal(mode, "file")) {
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
	if (zend_string_equals_literal(mode, "stream")) {
		part->source.kind = mpSTREAM;

		ZVAL_DUP(&part->source.zval, source);
		convert_to_string_ex(&part->source.zval);
	}

	/* parse the data from the source */
	if (part->source.kind == mpSTRING) {
		php_mimepart_parse(part, Z_STRVAL_P(&part->source.zval), Z_STRLEN_P(&part->source.zval));
	} else if (part->source.kind == mpSTREAM) {
		php_stream *srcstream;
		char buf[1024];

		php_stream_from_zval(srcstream, &part->source.zval);

		php_stream_rewind(srcstream);
		while(!php_stream_eof(srcstream)) {
			size_t n = php_stream_read(srcstream, buf, sizeof(buf));
			if (n > 0)
				php_mimepart_parse(part, buf, n);
		}
	}

	mailparse_mimemessage_populate(part, object);
}

PHP_METHOD(mimemessage, remove)
{
	php_mimepart *part;

	part = mimemsg_get_object(getThis());
	if (part == NULL)
		RETURN_FALSE;

	php_mimepart_remove_from_parent(part);
}

PHP_METHOD(mimemessage, add_child)
{
	php_mimepart *part;

	part = mimemsg_get_object(getThis());
	if (part == NULL)
		RETURN_FALSE;

	php_mimepart_remove_from_parent(part);
}


PHP_METHOD(mimemessage, get_child_count)
{
	php_mimepart *part;

	part = mimemsg_get_object(getThis());
	if (part == NULL)
		RETURN_FALSE;

	RETURN_LONG(zend_hash_num_elements(&part->children));
}

PHP_METHOD(mimemessage, get_parent)
{
	php_mimepart *part;

	part = mimemsg_get_object(getThis());

	if (part && part->parent) {
		mailparse_mimemessage_export(part->parent, return_value);
	} else {
		RETURN_NULL();
	}
}

PHP_METHOD(mimemessage, get_child)
{
	php_mimepart *part, *foundpart;
	zval *item_to_find;

	if ((part = mimemsg_get_object(getThis())) == NULL) {
		RETURN_NULL();
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &item_to_find) == FAILURE) {
		RETURN_NULL();
	}

	if (Z_TYPE_P(item_to_find) == IS_STRING) {
		foundpart = php_mimepart_find_by_name(part, Z_STRVAL_P(item_to_find));
	} else if (Z_TYPE_P(item_to_find) == IS_LONG) {
		foundpart = php_mimepart_find_child_by_position(part, Z_LVAL_P(item_to_find));
	} else {
		RETURN_NULL();
	}

	if (!foundpart)	{
		RETURN_NULL();
	}

	mailparse_mimemessage_export(foundpart, return_value);
}

static void mailparse_mimemessage_extract(int flags, INTERNAL_FUNCTION_PARAMETERS)
{
	php_mimepart *part;
	zval *zarg = NULL;
	php_stream *srcstream, *deststream = NULL;
	zend_long mode = MAILPARSE_EXTRACT_OUTPUT;
	php_mimepart_extract_func_t callback = extract_callback_stdout;
	void *callback_data = NULL;

	part = mimemsg_get_object(getThis());

	RETVAL_NULL();

	if (part == NULL)
		return;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "|lz!", &mode, &zarg))
		return;

	switch(mode) {
		case MAILPARSE_EXTRACT_STREAM:
			if (zarg == NULL) {
				php_error_docref(NULL, E_WARNING, "Parameter 2 must be a stream");
				return;
			}

			php_stream_from_zval(deststream, zarg);
			break;
		case MAILPARSE_EXTRACT_RETURN:
			deststream = php_stream_memory_create(TEMP_STREAM_DEFAULT);
			break;
	}


	if (part->source.kind == mpSTRING)
#if PHP_VERSION_ID < 80100
		srcstream = php_stream_memory_open(TEMP_STREAM_READONLY, Z_STRVAL_P(&part->source.zval), Z_STRLEN_P(&part->source.zval));
#else
		srcstream = php_stream_memory_open(TEMP_STREAM_READONLY, Z_STR(part->source.zval));
#endif
	else
		php_stream_from_zval(srcstream, &part->source.zval);

	if (srcstream == NULL) {
		php_error_docref(NULL, E_WARNING, "MimeMessage object is missing a source stream!");
		goto cleanup;
	}

	if (deststream != NULL) {
		callback_data = deststream;
		callback = extract_callback_stream;
	}

	if (SUCCESS == extract_part(part, flags, srcstream, callback_data, callback)) {

		if (mode == MAILPARSE_EXTRACT_RETURN) {
#if PHP_VERSION_ID < 80100
			size_t len;
			char *buf;

			buf = php_stream_memory_get_buffer(deststream, &len);
			RETVAL_STRINGL(buf, len);
#else
			RETVAL_STR_COPY(php_stream_memory_get_buffer(deststream));
#endif
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

PHP_METHOD(mimemessage, extract_headers)
{
	mailparse_mimemessage_extract(MAILPARSE_DECODE_NOBODY, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_METHOD(mimemessage, extract_body)
{
	mailparse_mimemessage_extract(MAILPARSE_DECODE_NOHEADERS | MAILPARSE_DECODE_8BIT, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_METHOD(mimemessage, extract_uue)
{
	php_mimepart *part;
	zval *zarg = NULL;
	php_stream *srcstream, *deststream = NULL;
	zend_long mode = MAILPARSE_EXTRACT_OUTPUT;
	zend_long index = 0; /* which uue to extract */
	off_t end;
	off_t start_pos;
	char buffer[4096];
	int nparts = 0;

	part = mimemsg_get_object(getThis());

	RETVAL_NULL();

	if (part == NULL) {
		return;
	}

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "l|lz", &index, &mode, &zarg)) {
		return;
	}

	switch(mode) {
		case MAILPARSE_EXTRACT_STREAM:
			if (zarg == NULL) {
				php_error_docref(NULL, E_WARNING, "Parameter 2 must be a stream");
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
#if PHP_VERSION_ID < 80100
		srcstream = php_stream_memory_open(TEMP_STREAM_READONLY, Z_STRVAL_P(&part->source.zval), Z_STRLEN_P(&part->source.zval));
#else
		srcstream = php_stream_memory_open(TEMP_STREAM_READONLY, Z_STR(part->source.zval));
#endif
	else
		php_stream_from_zval(srcstream, &part->source.zval);

	if (srcstream == NULL) {
		php_error_docref(NULL, E_WARNING, "MimeMessage object is missing a source stream!");
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
				mailparse_do_uudecode(srcstream, deststream);
				if (mode == MAILPARSE_EXTRACT_RETURN) {
#if PHP_VERSION_ID < 80100
					size_t len;
					char *buf;

					buf = php_stream_memory_get_buffer(deststream, &len);
					RETVAL_STRINGL(buf, len);
#else
					RETVAL_STR_COPY(php_stream_memory_get_buffer(deststream));
#endif
				} else {
					RETVAL_TRUE;
				}

				break;
			} else {
				/* skip that part */
				mailparse_do_uudecode(srcstream, NULL);
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

PHP_METHOD(mimemessage, enum_uue)
{
	php_stream *instream;
	php_mimepart *part;
	off_t end;
	off_t start_pos, curr_pos;
	size_t file_size;
	char buffer[4096];
	int nparts = 0;
	zval item;

	part = mimemsg_get_object(getThis());

	RETVAL_FALSE;

	if (part == NULL)
		return;

	if (part->source.kind == mpSTRING)
#if PHP_VERSION_ID < 80100
		instream = php_stream_memory_open(TEMP_STREAM_READONLY, Z_STRVAL_P(&part->source.zval), Z_STRLEN_P(&part->source.zval));
#else
		instream = php_stream_memory_open(TEMP_STREAM_READONLY, Z_STR(part->source.zval));
#endif
	else
		php_stream_from_zval(instream, &part->source.zval);

	if (instream == NULL) {
		php_error_docref(NULL, E_WARNING, "MimeMessage object is missing a source stream!");
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
			file_size = mailparse_do_uudecode(instream, NULL);
			add_assoc_long(&item, "filesize", file_size);

			curr_pos = php_stream_tell(instream);

			if (curr_pos > end) {
				/* we somehow overran the message boundary; the message itself is
				 * probably bogus, so lets cancel this item */
				php_error_docref(NULL, E_WARNING, "uue attachment overran part boundary; this should not happen, message is probably malformed");
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
static size_t mailparse_do_uudecode(php_stream *instream, php_stream *outstream)
{
	int x, A, B, C, D, n;
	size_t file_size = 0;
	char line[128];
	int backtick_line = 0;

	if (outstream) {
		/* write to outstream */
		while(!php_stream_eof(instream))	{
			if (!php_stream_gets(instream, line, sizeof(line))
				|| (backtick_line && strncmp(line, "end", 3) == 0 && (line[3] == '\r' || line[3] == '\n'))
			) {
				break;
			}
			backtick_line = line[0] == '`' && (line[1] == '\r' || line[1] == '\n');
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

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "r", &file))
		return;

	php_stream_from_zval(instream, file);

	outstream = _mailparse_create_stream(&outpath);
	if (outstream == NULL)	{
		zend_error(E_WARNING, "%s(): unable to open temp file", get_active_function_name());
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
				add_assoc_string(&item, "filename", ZSTR_VAL(outpath));
				add_next_index_zval(return_value, &item);
				zend_string_release(outpath);
			}

			/* add an item */
			array_init(&item);
			add_assoc_string(&item, "origfilename", origfilename);

			/* create a temp file for the data */
			partstream = _mailparse_create_stream(&outpath);
			if (partstream)	{
				nparts++;
				add_assoc_string(&item, "filename", ZSTR_VAL(outpath));
				add_next_index_zval(return_value, &item);

				/* decode it */
				mailparse_do_uudecode(instream, partstream);
				php_stream_close(partstream);
				zend_string_release(outpath);
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
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto array mailparse_rfc822_parse_addresses(string addresses)
   Parse addresses and returns a hash containing that data */
PHP_FUNCTION(mailparse_rfc822_parse_addresses)
{
	zend_string *addresses;
	php_rfc822_tokenized_t *toks = NULL;
	php_rfc822_addresses_t *addrs = NULL;
	int i;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &addresses) == FAILURE) {
		RETURN_FALSE;
	}
	toks = php_mailparse_rfc822_tokenize((const char*)ZSTR_VAL(addresses), 1);
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

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &file) == FAILURE)	{
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

static int mailparse_stream_output(int c, void *stream)
{
	char buf = c;

	return php_stream_write((php_stream*)stream, &buf, 1);
}
static int mailparse_stream_flush(void *stream)
{
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

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rrS", &srcfile, &destfile, &encod) == FAILURE)	{
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

	enc = mbfl_name2no_encoding(ZSTR_VAL(encod));
	if (enc == mbfl_no_encoding_invalid)	{
		zend_error(E_WARNING, "%s(): unknown encoding \"%s\"",
				get_active_function_name(),
				ZSTR_VAL(encod)
				);
		RETURN_FALSE;
	}

	buf = emalloc(bufsize);
	RETVAL_TRUE;

#if PHP_VERSION_ID >= 70300
	conv = mbfl_convert_filter_new(mbfl_no2encoding(mbfl_no_encoding_8bit),
			mbfl_no2encoding(enc),
			mailparse_stream_output,
			mailparse_stream_flush,
			deststream
			);
#else
	conv = mbfl_convert_filter_new(mbfl_no_encoding_8bit,
			enc,
			mailparse_stream_output,
			mailparse_stream_flush,
			deststream
			);
#endif

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
					mbfl_convert_filter_flush(conv);
					php_stream_write(deststream, "=46rom ", 7);
					i = 5;
				} else {
					i = 0;
				}

				for (; i<len; i++)
					mbfl_convert_filter_feed(buf[i], conv);
			}
		}

	} else {
		while(!php_stream_eof(srcstream))	{
			len = php_stream_read(srcstream, buf, bufsize);
			if (len > 0)
			{
				size_t i;
				for (i=0; i<len; i++)
					mbfl_convert_filter_feed(buf[i], conv);
			}
		}
	}

	mbfl_convert_filter_flush(conv);
	mbfl_convert_filter_delete(conv);
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

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rS", &arg, &data) == FAILURE)	{
		RETURN_FALSE;
	}

	mailparse_fetch_mimepart_resource(part, arg);

	if (FAILURE == php_mimepart_parse(part, ZSTR_VAL(data), ZSTR_LEN(data))) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto resource mailparse_msg_parse_file(string filename)
   Parse file and return a resource representing the structure */
PHP_FUNCTION(mailparse_msg_parse_file)
{
	zend_string *filename;
	php_mimepart *part;
	char *filebuf;
	php_stream *stream;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &filename) == FAILURE)	{
		RETURN_FALSE;
	}

	/* open file and read it in */
	stream = php_stream_open_wrapper(ZSTR_VAL(filename), "rb", REPORT_ERRORS, NULL);
	if (stream == NULL)	{
		RETURN_FALSE;
	}

	filebuf = emalloc(MAILPARSE_BUFSIZ);

	part = php_mimepart_alloc();
	php_mimepart_to_zval(return_value, part->rsrc);

	while(!php_stream_eof(stream))	{
		int got = php_stream_read(stream, filebuf, MAILPARSE_BUFSIZ);
		if (got > 0)	{
			if (FAILURE == php_mimepart_parse(part, filebuf, got)) {
				/* We have to destroy the already allocated part, if we not return it */
				php_mimepart_free(part);
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
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &arg) == FAILURE)	{
		RETURN_FALSE;
	}
	zend_list_close(Z_RES_P(arg));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int mailparse_msg_create(void)
   Returns a handle that can be used to parse a message */
PHP_FUNCTION(mailparse_msg_create)
{
	php_mimepart *part = php_mimepart_alloc();

	RETURN_RES(part->rsrc);
}
/* }}} */

static int get_structure_callback(php_mimepart *part, php_mimepart_enumerator *id, void *ptr)
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
			zend_error(E_WARNING, "%s(): too many nested sections in message", get_active_function_name());
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
	efree(buf);
	return SUCCESS;
}

/* {{{ proto array mailparse_msg_get_structure(resource mimepart)
   Returns an array of mime section names in the supplied message */
PHP_FUNCTION(mailparse_msg_get_structure)
{
	zval *arg;
	php_mimepart *part;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &arg) == FAILURE)	{
		RETURN_FALSE;
	}

	mailparse_fetch_mimepart_resource(part, arg);

	array_init(return_value);
	php_mimepart_enum_parts(part, &get_structure_callback, return_value);
}
/* }}} */

/* callback for decoding using a "userdefined" php function */
static int extract_callback_user_func(php_mimepart *part, zval *userfunc, const char *p, size_t n)
{
	zval retval, arg;
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;

	ZVAL_STRINGL(&arg, (char*)p, (int)n);

	if (zend_fcall_info_init(userfunc, 0, &fci, &fcc, NULL, NULL) == FAILURE) {
		zend_error(E_WARNING, "%s(): unable to call user function", get_active_function_name());
		return 0;
	}

	zend_fcall_info_argn(&fci, 1, &arg);
	fci.retval = &retval;
	if (zend_call_function(&fci, &fcc)) {
		zend_fcall_info_args_clear(&fci, 1);
		zend_error(E_WARNING, "%s(): unable to call user function", get_active_function_name());
		return 0;
	}

	zend_fcall_info_args_clear(&fci, 1);
	zval_ptr_dtor(&retval);
	zval_ptr_dtor(&arg);

	return 0;
}

/* callback for decoding to the current output buffer */
static int extract_callback_stdout(php_mimepart *part, void *ptr, const char *p, size_t n)
{
	ZEND_WRITE(p, n);
	return 0;
}

/* callback for decoding to a stream */
static int extract_callback_stream(php_mimepart *part, void *ptr, const char *p, size_t n)
{
	php_stream_write((php_stream*)ptr, p, n);
	return 0;
}

#define MAILPARSE_DECODE_NONE		0		/* include headers and leave section untouched */
#define MAILPARSE_DECODE_8BIT		1		/* decode body into 8-bit */
#define MAILPARSE_DECODE_NOHEADERS	2		/* don't include the headers */
#define MAILPARSE_DECODE_NOBODY		4		/* don't include the body */

static int extract_part(php_mimepart *part, int decode, php_stream *src, void *callbackdata,
		php_mimepart_extract_func_t callback)
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

	php_mimepart_decoder_prepare(part, decode & MAILPARSE_DECODE_8BIT, callback, callbackdata);

	if (php_stream_seek(src, start_pos, SEEK_SET) == -1) {
		zend_error(E_WARNING, "%s(): unable to seek to section start", get_active_function_name());
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
			zend_error(E_WARNING, "%s(): error reading from file at offset %ld", get_active_function_name(), start_pos);
			goto cleanup;
		}

		filebuf[n] = '\0';

		php_mimepart_decoder_feed(part, filebuf, n);

		start_pos += n;
	}
	ret = SUCCESS;

cleanup:
	php_mimepart_decoder_finish(part);

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

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "rz|z", &zpart, &filename, &callbackfunc)) {
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
#if PHP_VERSION_ID < 80100
		srcstream = php_stream_memory_open(TEMP_STREAM_READONLY, Z_STRVAL_P(filename), Z_STRLEN_P(filename));
#else
		srcstream = php_stream_memory_open(TEMP_STREAM_READONLY, Z_STR_P(filename));
#endif
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
			php_stream_from_zval(deststream, callbackfunc);
			cbfunc = extract_callback_stream;
			cbdata = deststream;
			deststream = NULL; /* don't free this one */
		} else {
			cbfunc = (php_mimepart_extract_func_t)&extract_callback_user_func;
			cbdata = callbackfunc;
		}
	} else {
		cbfunc = extract_callback_stdout;
		cbdata = NULL;
	}

	RETVAL_FALSE;

	if (SUCCESS == extract_part(part, decode, srcstream, cbdata, cbfunc)) {

		if (deststream != NULL) {
#if PHP_VERSION_ID < 80100
			/* return it's contents as a string */
			char *membuf = NULL;
			size_t memlen = 0;
			membuf = php_stream_memory_get_buffer(deststream, &memlen);
			RETVAL_STRINGL(membuf, memlen);
#else
			RETVAL_STR_COPY(php_stream_memory_get_buffer(deststream));
#endif
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
		struct php_mimeheader_with_attributes *attr)
{
	HashPosition pos;
	zval *val;
	char *newkey;
	zend_ulong num_index;
	zend_string *str_key;

	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(&attr->attributes), &pos);
	while ((val = zend_hash_get_current_data_ex(Z_ARRVAL_P(&attr->attributes), &pos)) != NULL) {

		zend_hash_get_current_key_ex(Z_ARRVAL_P(&attr->attributes), &str_key, &num_index, &pos);

    if (str_key) {
      spprintf(&newkey, 0, "%s%s", attrprefix, ZSTR_VAL(str_key));
    } else {
      spprintf(&newkey, 0, "%s" ZEND_ULONG_FMT, attrprefix, num_index);
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

static void add_header_reference_to_zval(char *headerkey, zval *return_value, zval *headers)
{
	zval *headerval, newhdr;
	zend_string *hash_key;

	hash_key = zend_string_init(headerkey, strlen(headerkey), 0);
	if ((headerval = zend_hash_find(Z_ARRVAL_P(headers), hash_key)) != NULL) {
		ZVAL_DUP(&newhdr, headerval);
		add_assoc_zval(return_value, headerkey, &newhdr);
	}
	zend_string_release(hash_key);
}

static int mailparse_get_part_data(php_mimepart *part, zval *return_value)
{
	zval headers, *tmpval;
	off_t startpos, endpos, bodystart;
	int nlines, nbodylines;
	/* extract the address part of the content-id only */
	zend_string *hash_key = zend_string_init("content-id", sizeof("content-id") - 1, 0);

	array_init(return_value);

	/* get headers for this section */
	ZVAL_COPY(&headers, &part->headerhash);

	add_assoc_zval(return_value, "headers", &headers);

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
		add_attr_header_to_zval("content-type", "content-", return_value, part->content_type);
	else
		add_assoc_string(return_value, "content-type", "text/plain; (error)");

	if (part->content_disposition)
		add_attr_header_to_zval("content-disposition", "disposition-", return_value, part->content_disposition);

	if (part->content_location)
		add_assoc_string(return_value, "content-location", part->content_location);

	if (part->content_base)
		add_assoc_string(return_value, "content-base", part->content_base);
	else
		add_assoc_string(return_value, "content-base", "/");

	if (part->boundary)
		add_assoc_string(return_value, "content-boundary", part->boundary);

	if ((tmpval = zend_hash_find(Z_ARRVAL_P(&headers), hash_key)) != NULL) {
		php_rfc822_tokenized_t *toks;
		php_rfc822_addresses_t *addrs;

		toks = php_mailparse_rfc822_tokenize(Z_STRVAL_P(tmpval), 1);
		addrs = php_rfc822_parse_address_tokens(toks);
		if (addrs->naddrs > 0)
			add_assoc_string(return_value, "content-id", addrs->addrs[0].address);
		php_rfc822_free_addresses(addrs);
		php_rfc822_tokenize_free(toks);
	}
	zend_string_release(hash_key);

	add_header_reference_to_zval("content-description", return_value, &headers);
	add_header_reference_to_zval("content-language", return_value, &headers);
	add_header_reference_to_zval("content-md5", return_value, &headers);

	return SUCCESS;
}

/* {{{ proto array mailparse_msg_get_part_data(resource mimepart)
   Returns an associative array of info about the message */
PHP_FUNCTION(mailparse_msg_get_part_data)
{
	zval *arg;
	php_mimepart *part;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &arg) == FAILURE)	{
		RETURN_FALSE;
	}

	mailparse_fetch_mimepart_resource(part, arg);

	mailparse_get_part_data(part, return_value);
}
/* }}} */

/* {{{ proto int mailparse_msg_get_part(resource mimepart, string mimesection)
   Returns a handle on a given section in a mimemessage */
PHP_FUNCTION(mailparse_msg_get_part)
{
	zval *arg;
	php_mimepart *part, *foundpart;
	zend_string *mimesection;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rS", &arg, &mimesection) == FAILURE)	{
		RETURN_FALSE;
	}

	mailparse_fetch_mimepart_resource(part, arg);

	foundpart = php_mimepart_find_by_name(part, ZSTR_VAL(mimesection));

	if (!foundpart)	{
		php_error_docref(NULL, E_WARNING, "cannot find section %s in message", ZSTR_VAL(mimesection));
		RETURN_FALSE;
	}
	foundpart->rsrc->gc.refcount++;
	php_mimepart_to_zval(return_value, foundpart->rsrc);
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
