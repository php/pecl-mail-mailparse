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
*/

/*
 * Vendored encoding support for mailparse
 *
 * This code is derived from libmbfl, as modified in the mbstring extension,
 * and has been adapted for standalone use in mailparse.
 *
 * Original libmbfl code copyright (c) 1998-2002 HappySize, Inc.
 * Licensed under GNU Lesser General Public License (version 2)
 */

#ifndef MAILPARSE_ENCODING_H
#define MAILPARSE_ENCODING_H

#include "php.h"

/* Encoding identifiers */
enum mb_no_encoding {
	mb_no_encoding_invalid = -1,
	mb_no_encoding_7bit,
	mb_no_encoding_8bit,
	mb_no_encoding_base64,
	mb_no_encoding_qprint
};

/* Forward declarations */
typedef struct _mb_convert_filter mb_convert_filter;
typedef struct _mb_encoding mb_encoding;
typedef struct mb_convert_vtbl mb_convert_vtbl;

/* Function pointer types */
typedef int (*mb_output_function_t)(int c, void *data);
typedef int (*mb_flush_function_t)(void *data);
typedef int (*mb_filter_function_t)(int c, mb_convert_filter *filter);
typedef int (*mb_filter_flush_t)(mb_convert_filter *filter);
typedef void (*mb_filter_ctor_t)(mb_convert_filter *filter);
typedef void (*mb_filter_dtor_t)(mb_convert_filter *filter);

/* Encoding structure */
struct _mb_encoding {
	enum mb_no_encoding no_encoding;
	const char *name;
};

/* Virtual table for convert filters */
struct mb_convert_vtbl {
	enum mb_no_encoding from;
	enum mb_no_encoding to;
	mb_filter_ctor_t filter_ctor;
	mb_filter_dtor_t filter_dtor;
	mb_filter_function_t filter_function;
	mb_filter_flush_t filter_flush;
};

/* Convert filter structure */
struct _mb_convert_filter {
	mb_filter_dtor_t filter_dtor;
	mb_filter_function_t filter_function;
	mb_filter_flush_t filter_flush;
	mb_output_function_t output_function;
	mb_flush_function_t flush_function;
	void *data;
	int status;
	int cache;
	const mb_encoding *from;
	const mb_encoding *to;
};

/* Public API functions */
mb_convert_filter* mb_convert_filter_new(
	const mb_encoding *from,
	const mb_encoding *to,
	mb_output_function_t output_function,
	mb_flush_function_t flush_function,
	void *data
);

void mb_convert_filter_delete(mb_convert_filter *filter);
int mb_convert_filter_feed(int c, mb_convert_filter *filter);
int mb_convert_filter_flush(mb_convert_filter *filter);

const mb_encoding* mb_name2encoding(const char *name);
const mb_encoding* mb_no2encoding(enum mb_no_encoding no_encoding);

#endif /* MAILPARSE_ENCODING_H */
