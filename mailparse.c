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

#include "ext/mbstring/mbfilter.h"

static int le_mime_part;

function_entry mailparse_functions[] = {
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
    NO_VERSION_YET,
	STANDARD_MODULE_PROPERTIES
};

ZEND_DECLARE_MODULE_GLOBALS(mailparse)

#ifdef COMPILE_DL_MAILPARSE
ZEND_GET_MODULE(mailparse)
#endif

ZEND_RSRC_DTOR_FUNC(mimepart_dtor)
{
	php_mimepart *part = rsrc->ptr;

	if (part->parent == NULL)
		php_mimepart_free(part);
}
	
PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("mailparse.def_charset", "us-ascii", PHP_INI_ALL, OnUpdateString, def_charset, zend_mailparse_globals, mailparse_globals)
PHP_INI_END()

#define mailparse_msg_name	"mailparse_mail_structure"

#define mailparse_fetch_mimepart_resource(rfcvar, zvalarg)	ZEND_FETCH_RESOURCE(rfcvar, php_mimepart *, zvalarg, -1, mailparse_msg_name, le_mime_part)

PHPAPI int php_mailparse_le_mime_part(void)
{
	return le_mime_part;
}

PHP_MINIT_FUNCTION(mailparse)
{
#ifdef ZTS
	zend_mailparse_globals *mailparse_globals;

	ts_allocate_id(&mailparse_globals_id, sizeof(zend_mailparse_globals), NULL, NULL);
	mailparse_globals = ts_resource(mailparse_globals_id);
#endif

	le_mime_part = 		zend_register_list_destructors_ex(mimepart_dtor, NULL, mailparse_msg_name, module_number);

	REGISTER_INI_ENTRIES();

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(mailparse)
{
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}

PHP_MINFO_FUNCTION(mailparse)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "mailparse support", "enabled");
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
#define UU_NEXT(v)	v = php_stream_getc(instream); if (v == EOF) break; v = UUDEC(v)
static void mailparse_do_uudecode(php_stream * instream, php_stream * outstream TSRMLS_DC)
{
	int A, B, C, D, n;

	while(!php_stream_eof(instream))	{
		UU_NEXT(n);

		while(n != 0)	{
			UU_NEXT(A);
			UU_NEXT(B);
			UU_NEXT(C);
			UU_NEXT(D);

			if (n-- > 0)
				php_stream_putc(outstream, (A << 2) | (B >> 4));
			if (n-- > 0)
				php_stream_putc(outstream, (B << 4) | (C >> 2));
			if (n-- > 0)
				php_stream_putc(outstream, (C << 6) | D);
		}
		/* skip newline */
		php_stream_getc(instream);
	}
}


/* {{{ proto array mailparse_uudecode_all(resource fp)
   Scans the data from fp and extract each embedded uuencoded file. Returns an array listing filename information */
PHP_FUNCTION(mailparse_uudecode_all)
{
	zval * file, * item;
	int type;
	char * buffer = NULL;
	char * outpath = NULL;
	int nparts = 0;
	php_stream * instream, *outstream = NULL, *partstream = NULL;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &file))
		return;

	instream = (php_stream*)zend_fetch_resource(&file TSRMLS_CC, -1, "File-Handle", &type, 1, php_file_le_stream());
	ZEND_VERIFY_RESOURCE(instream);

	outstream = php_stream_fopen_temporary_file(NULL, "mailparse", &outpath);
	if (outstream == NULL)	{
		zend_error(E_WARNING, "%s(): unable to open temp file", get_active_function_name(TSRMLS_C));
		RETURN_FALSE;
	}

	php_stream_rewind(instream);
	
	buffer = emalloc(4096);
	while(php_stream_gets(instream, buffer, 4096))	{
		/* Look for the "begin " sequence that identifies a uuencoded file */
		if (strncmp(buffer, "begin ", 6) == 0)	{
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
			if (nparts == 0)	{
				array_init(return_value);
				/* create an initial item representing the file with all uuencoded parts
				 * removed */
				MAKE_STD_ZVAL(item);
				array_init(item);
				add_assoc_string(item, "filename", outpath, 0);
				add_next_index_zval(return_value, item);
			}
			
			/* add an item */
			MAKE_STD_ZVAL(item);
			array_init(item);
			add_assoc_string(item, "origfilename", origfilename, 1);

			/* create a temp file for the data */
			partstream = php_stream_fopen_temporary_file(NULL, "mailparse", &outpath);
			if (partstream)	{
				nparts++;
				add_assoc_string(item, "filename", outpath, 0);
				add_next_index_zval(return_value, item);

				/* decode it */
				mailparse_do_uudecode(instream, partstream TSRMLS_CC);
				php_stream_close(partstream);
			}
		}
		else	{
			/* write to the output file */
			php_stream_write_string(outstream, buffer);
		}
	}
	php_stream_close(outstream);
	php_stream_rewind(instream);
	efree(buffer);

	if (nparts == 0)	{
		/* delete temporary file */
		unlink(outpath);
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
	long addresses_len;
	php_rfc822_tokenized_t *toks;
	php_rfc822_addresses_t *addrs;
	int i;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &addresses, &addresses_len) == FAILURE) {
		RETURN_FALSE;
	}
	toks = php_mailparse_rfc822_tokenize((const char*)addresses, 1 TSRMLS_CC);
	addrs = php_rfc822_parse_address_tokens(toks);

	array_init(return_value);

	for (i = 0; i < addrs->naddrs; i++) {
		zval *item;

		MAKE_STD_ZVAL(item);
		array_init(item);

		if (addrs->addrs[i].name)
			add_assoc_string(item, "display", addrs->addrs[i].name, 1);
		if (addrs->addrs[i].address)
			add_assoc_string(item, "address", addrs->addrs[i].address, 1);
		add_assoc_bool(item, "is_group", addrs->addrs[i].is_group);

		zend_hash_next_index_insert(Z_ARRVAL_P(return_value), &item, sizeof(item), NULL);
	}
	
	php_rfc822_free_addresses(addrs);
	php_rfc822_tokenize_free(toks);	
}
/* }}} */

/* {{{ proto string mailparse_determine_best_xfer_encoding(resource fp)
   Figures out the best way of encoding the content read from the file pointer fp, which must be seek-able */
PHP_FUNCTION(mailparse_determine_best_xfer_encoding)
{
	zval ** file;
	int longline = 0;
	int linelen = 0;
	int c;
	enum mbfl_no_encoding bestenc = mbfl_no_encoding_7bit;
	php_stream *stream;
	char * name;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &file) == FAILURE)	{
		WRONG_PARAM_COUNT;
	}

	stream = zend_fetch_resource(file TSRMLS_CC, -1, "File-Handle", NULL, 1, php_file_le_stream());
	ZEND_VERIFY_RESOURCE(stream);

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
		RETVAL_STRING(name, 1);
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
	TSRMLS_FETCH();

	return php_stream_write((php_stream*)stream, &buf, 1);
}
static int mailparse_stream_flush(void *stream)
{
	TSRMLS_FETCH();
	return php_stream_flush((php_stream*)stream);
}

PHP_FUNCTION(mailparse_stream_encode)
{
	zval **srcfile, **destfile, **encod;
	void *what;
	int type;
	php_stream *srcstream, *deststream;
	char *buf;
	size_t len;
	size_t bufsize = 2048;
	enum mbfl_no_encoding enc;
	mbfl_convert_filter *conv = NULL;

	if (ZEND_NUM_ARGS() != 3 || zend_get_parameters_ex(3, &srcfile, &destfile, &encod) == FAILURE)	{
		WRONG_PARAM_COUNT;
	}

	if (Z_TYPE_PP(srcfile) == IS_RESOURCE && Z_LVAL_PP(srcfile) == 0)	{
		RETURN_FALSE;
	}
	if (Z_TYPE_PP(destfile) == IS_RESOURCE && Z_LVAL_PP(destfile) == 0)	{
		RETURN_FALSE;
	}

	what = zend_fetch_resource(srcfile TSRMLS_CC, -1, "File-Handle", &type, 1, php_file_le_stream());
	ZEND_VERIFY_RESOURCE(what);

	srcstream = (php_stream*)what;

	what = zend_fetch_resource(destfile TSRMLS_CC, -1, "File-Handle", &type, 1, php_file_le_stream());
	ZEND_VERIFY_RESOURCE(what);
	deststream = (php_stream*)what;

	convert_to_string_ex(encod);
	enc = mbfl_name2no_encoding(Z_STRVAL_PP(encod));
	if (enc == mbfl_no_encoding_invalid)	{
		zend_error(E_WARNING, "%s(): unknown encoding \"%s\"",
				get_active_function_name(TSRMLS_C),
				Z_STRVAL_PP(encod)
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
			);

	if (enc == mbfl_no_encoding_qprint) {
		/* If the qp encoded section is going to be digitally signed,
		 * it is a good idea to make sure that lines that begin "From "
		 * have the letter F encoded, so that MTAs do not stick a > character
		 * in front of it and invalidate the content/signature */
		while(!php_stream_eof(srcstream))	{
			if (NULL != php_stream_gets(srcstream, buf, bufsize)) {
				int i;
				
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
				int i;
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
	char *data;
	long data_len;
	zval *arg;
	php_mimepart *part;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &arg, &data, &data_len) == FAILURE)	{
		RETURN_FALSE;
	}

	mailparse_fetch_mimepart_resource(part, &arg);

	php_mimepart_parse(part, data, data_len);
}
/* }}} */

/* {{{ proto resource mailparse_msg_parse_file(string filename)
   Parse file and return a resource representing the structure */
PHP_FUNCTION(mailparse_msg_parse_file)
{
	char *filename;
	long filename_len;
	php_mimepart *part;
	char *filebuf;
	php_stream *stream;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &filename, &filename_len) == FAILURE)	{
		RETURN_FALSE;
	}

	/* open file and read it in */
	stream = php_stream_open_wrapper(filename, "rb", ENFORCE_SAFE_MODE|REPORT_ERRORS, NULL);
	if (stream == NULL)	{
		RETURN_FALSE;
	}

	filebuf = emalloc(MAILPARSE_BUFSIZ);

	part = php_mimepart_alloc();
	php_mimepart_to_zval(return_value, part);

	while(!php_stream_eof(stream))	{
		int got = php_stream_read(stream, filebuf, MAILPARSE_BUFSIZ);
		if (got > 0)	{
			php_mimepart_parse(part, filebuf, got);
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

	mailparse_fetch_mimepart_resource(part, &arg);
	zend_list_delete(Z_LVAL_P(arg));
	RETURN_TRUE;
}
/* }}} */


/* {{{ proto int mailparse_msg_create(void)
   Returns a handle that can be used to parse a message */
PHP_FUNCTION(mailparse_msg_create)
{
	php_mimepart *part = php_mimepart_alloc();

	php_mimepart_to_zval(return_value, part);
}
/* }}} */

static int get_structure_callback(php_mimepart *part, php_mimepart_enumerator *id, void *ptr TSRMLS_DC)
{
	zval *return_value = (zval *)ptr;
	char intbuf[16];
	char buf[256];
	int len, i = 0;

	while(id && i < sizeof(buf))	{
		sprintf(intbuf, "%d", id->id);
		len = strlen(intbuf);
		if (len > (sizeof(buf)-i))	{
			/* too many sections: bail */
			zend_error(E_WARNING, "%s(): too many nested sections in message", get_active_function_name(TSRMLS_C));
			return FAILURE;
		}
		sprintf(&buf[i], "%s%c", intbuf, id->next ? '.' : '\0');
		i += len + (id->next ? 1 : 0);
		id = id->next;
	}
	add_next_index_string(return_value, buf,1);
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

	mailparse_fetch_mimepart_resource(part, &arg);

	if (array_init(return_value) == FAILURE)	{
		RETURN_FALSE;
	}
	php_mimepart_enum_parts(part, &get_structure_callback, return_value TSRMLS_CC);
}
/* }}} */

/* callback for decoding using a "userdefined" php function */
static int extract_callback_user_func(php_mimepart *part, zval *userfunc, const char *p, size_t n TSRMLS_DC)
{
	zval *retval;
	zval *arg;

	MAKE_STD_ZVAL(retval);
	Z_TYPE_P(retval) = IS_BOOL;
	Z_LVAL_P(retval) = 0;

	MAKE_STD_ZVAL(arg);
	ZVAL_STRINGL(arg, (char*)p, (int)n, 1);

	/* TODO: use zend_is_callable */

	if (call_user_function(EG(function_table), NULL, userfunc, retval, 1, &arg TSRMLS_CC) == FAILURE)
		zend_error(E_WARNING, "%s(): unable to call user function", get_active_function_name(TSRMLS_C));

	zval_dtor(retval);
	zval_dtor(arg);
	efree(retval);
	efree(arg);

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


#define MAILPARSE_DECODE_NONE	0		/* include headers and leave section untouched */
#define MAILPARSE_DECODE_8BIT	1		/* decode body into 8-bit */
#define MAILPARSE_DECODE_NOHEADERS	2	/* don't include the headers */
static int extract_part(php_mimepart *part, int decode, php_stream *src, void *callbackdata,
		php_mimepart_extract_func_t callback TSRMLS_DC)
{
	off_t end;
	off_t start_pos;
	char *filebuf = NULL;
	int ret = FAILURE;
	
	/* figure out where the message part starts/ends */
	start_pos = decode & MAILPARSE_DECODE_NOHEADERS ? part->bodystart : part->startpos;
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
			zend_error(E_WARNING, "%s(): error reading from file at offset %d", get_active_function_name(TSRMLS_C), start_pos);
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

	mailparse_fetch_mimepart_resource(part, &zpart);

	/* filename can be a filename or a stream */
	if (Z_TYPE_P(filename) == IS_RESOURCE) {
		srcstream = (php_stream*)zend_fetch_resource(&filename TSRMLS_CC, -1, "File-Handle", NULL, 1, php_file_le_stream());
		ZEND_VERIFY_RESOURCE(srcstream);
	} else if (isfile) {
		convert_to_string_ex(&filename);
		srcstream = php_stream_open_wrapper(Z_STRVAL_P(filename), "rb", ENFORCE_SAFE_MODE|REPORT_ERRORS, NULL);
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
			deststream = (php_stream*)zend_fetch_resource(&callbackfunc TSRMLS_CC, -1, "File-Handle", NULL, 1, php_file_le_stream());
			ZEND_VERIFY_RESOURCE(deststream);
			cbfunc = extract_callback_stream;
			cbdata = deststream;
			deststream = NULL; /* don't free this one */
		} else {
			if (Z_TYPE_P(callbackfunc) != IS_ARRAY)
				convert_to_string_ex(&callbackfunc);
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
			RETVAL_STRINGL(membuf, memlen, 1);

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
	zval **val;
	char *key, *newkey;
	int key_len, pref_len;

	pref_len = strlen(attrprefix);
	
	add_assoc_string(return_value, valuelabel, attr->value, 1);

	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(attr->attributes), &pos);
	while (SUCCESS == zend_hash_get_current_data_ex(Z_ARRVAL_P(attr->attributes), (void**)&val, &pos)) {

		zend_hash_get_current_key_ex(Z_ARRVAL_P(attr->attributes), &key, &key_len, NULL, 0, &pos);

		newkey = emalloc(key_len + pref_len + 1);
		strcpy(newkey, attrprefix);
		strcat(newkey, key);

		add_assoc_string(return_value, newkey, Z_STRVAL_PP(val), 1);
		efree(newkey);
		
		zend_hash_move_forward_ex(Z_ARRVAL_P(attr->attributes), &pos);
	}
}

static void add_header_reference_to_zval(char *headerkey, zval *return_value, zval *headers TSRMLS_DC)
{
	zval **headerval;

	if (SUCCESS == zend_hash_find(Z_ARRVAL_P(headers), headerkey, strlen(headerkey)+1, (void**)&headerval)) {
		ZVAL_ADDREF(*headerval);
		zend_hash_update(Z_ARRVAL_P(return_value), headerkey, strlen(headerkey)+1, (void**)headerval, sizeof(zval *), NULL);
	}
}

/* {{{ proto array mailparse_msg_get_part_data(resource mimepart)
   Returns an associative array of info about the message */
PHP_FUNCTION(mailparse_msg_get_part_data)
{
	zval *arg;
	php_mimepart *part;
	zval *headers, **tmpval;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &arg) == FAILURE)	{
		RETURN_FALSE;
	}

	mailparse_fetch_mimepart_resource(part, &arg);

	array_init(return_value);
	
	/* get headers for this section */
	MAKE_STD_ZVAL(headers);
	*headers = *part->headerhash;
	INIT_PZVAL(headers);
	zval_copy_ctor(headers);

	add_assoc_zval(return_value, "headers", headers);

	add_assoc_long(return_value, "starting-pos",		part->startpos);
	add_assoc_long(return_value, "starting-pos-body",	part->bodystart);
	add_assoc_long(return_value, "ending-pos",			part->endpos);
	add_assoc_long(return_value, "ending-pos-body",		part->bodyend);
	add_assoc_long(return_value, "line-count",			part->nlines);
	add_assoc_long(return_value, "body-line-count",		part->nbodylines);

	add_assoc_string(return_value, "charset", part->charset, 1);
	
	if (part->content_transfer_encoding)
		add_assoc_string(return_value, "transfer-encoding", part->content_transfer_encoding, 1);
	
	if (part->content_type)
		add_attr_header_to_zval("content-type", "content-", return_value, part->content_type TSRMLS_CC);

	if (part->content_disposition)
		add_sttr_header_to_zval("content-disposition", "disposition-", return_value, part->content_disposition TSRMLS_CC);

	if (part->content_location)
		add_assoc_string(return_value, "content-location", part->content_location, 1);

	if (part->content_base)
		add_assoc_string(return_value, "content-base", part->content_base, 1);

	/* extract the address part of the content-id only */
	if (SUCCESS == zend_hash_find(Z_ARRVAL_P(headers), "content-id", sizeof("content-id"), (void**)&tmpval)) {
		php_rfc822_tokenized_t *toks;
		php_rfc822_addresses_t *addrs;

		toks = php_mailparse_rfc822_tokenize((const char*)Z_STRVAL_PP(tmpval), 1 TSRMLS_CC);
		addrs = php_rfc822_parse_address_tokens(toks);
		if (addrs->naddrs > 0)
			add_assoc_string(return_value, "content-id", addrs->addrs[0].address, 1);
		php_rfc822_free_addresses(addrs);
		php_rfc822_tokenize_free(toks);	
	}
	
	add_header_reference_to_zval("content-description", return_value, headers);
	add_header_reference_to_zval("content-language", return_value, headers);
	add_header_reference_to_zval("content-md5", return_value, headers);
		
}
/* }}} */


/* {{{ proto int mailparse_msg_get_part(resource mimepart, string mimesection)
   Returns a handle on a given section in a mimemessage */
PHP_FUNCTION(mailparse_msg_get_part)
{
	zval *arg;
	php_mimepart *part, *foundpart;
	char *mimesection;
	long mimesection_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &arg, &mimesection, &mimesection_len) == FAILURE)	{
		RETURN_FALSE;
	}

	mailparse_fetch_mimepart_resource(part, &arg);

	foundpart = php_mimepart_find_by_name(part, mimesection TSRMLS_CC);

	if (!foundpart)	{
		php_error(E_WARNING, "%s(): cannot find section %s in message", get_active_function_name(TSRMLS_C), mimesection);
		RETURN_FALSE;
	}
	zend_list_addref(foundpart->rsrc_id);
	php_mimepart_to_zval(return_value, foundpart);
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
