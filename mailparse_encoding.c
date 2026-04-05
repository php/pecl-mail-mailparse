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
 *
 * The source code includes portions from:
 * - mbfilter_base64.c (Base64 encoding/decoding)
 * - mbfilter_qprint.c (Quoted-Printable encoding/decoding)
 * - mbfl_convert.c (Filter infrastructure)
 */

#include "php.h"
#include "mailparse_encoding.h"
#include <string.h>

#define CK(statement)	do { if ((statement) < 0) return (-1); } while (0)

/* Forward declarations */
static int mb_filt_conv_base64enc(int c, mb_convert_filter *filter);
static int mb_filt_conv_base64enc_flush(mb_convert_filter *filter);
static int mb_filt_conv_base64dec(int c, mb_convert_filter *filter);
static int mb_filt_conv_base64dec_flush(mb_convert_filter *filter);
static int mb_filt_conv_qprintenc(int c, mb_convert_filter *filter);
static int mb_filt_conv_qprintenc_flush(mb_convert_filter *filter);
static int mb_filt_conv_qprintdec(int c, mb_convert_filter *filter);
static int mb_filt_conv_qprintdec_flush(mb_convert_filter *filter);
static void mb_filt_conv_common_ctor(mb_convert_filter *filter);

/* Encoding definitions */
static const mb_encoding mb_encoding_7bit = { mb_no_encoding_7bit, "7bit" };
static const mb_encoding mb_encoding_8bit = { mb_no_encoding_8bit, "8bit" };
static const mb_encoding mb_encoding_base64 = { mb_no_encoding_base64, "BASE64" };
static const mb_encoding mb_encoding_qprint = { mb_no_encoding_qprint, "Quoted-Printable" };

/* Virtual tables for conversions */
static const mb_convert_vtbl vtbl_8bit_b64 = {
	mb_no_encoding_8bit,
	mb_no_encoding_base64,
	mb_filt_conv_common_ctor,
	NULL,
	mb_filt_conv_base64enc,
	mb_filt_conv_base64enc_flush
};

static const mb_convert_vtbl vtbl_b64_8bit = {
	mb_no_encoding_base64,
	mb_no_encoding_8bit,
	mb_filt_conv_common_ctor,
	NULL,
	mb_filt_conv_base64dec,
	mb_filt_conv_base64dec_flush
};

static const mb_convert_vtbl vtbl_8bit_qprint = {
	mb_no_encoding_8bit,
	mb_no_encoding_qprint,
	mb_filt_conv_common_ctor,
	NULL,
	mb_filt_conv_qprintenc,
	mb_filt_conv_qprintenc_flush
};

static const mb_convert_vtbl vtbl_qprint_8bit = {
	mb_no_encoding_qprint,
	mb_no_encoding_8bit,
	mb_filt_conv_common_ctor,
	NULL,
	mb_filt_conv_qprintdec,
	mb_filt_conv_qprintdec_flush
};

/* =============================================================================
 * BASE64 encoding/decoding
 * ============================================================================= */

static const unsigned char mb_base64_table[] = {
	/* 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', */
	0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,
	/* 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', */
	0x4e,0x4f,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,
	/* 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', */
	0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,
	/* 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', */
	0x6e,0x6f,0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,
	/* '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', '\0' */
	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x2b,0x2f,0x00
};

/* any => BASE64 */
static int mb_filt_conv_base64enc(int c, mb_convert_filter *filter)
{
	int n;

	n = (filter->status & 0xff);
	if (n == 0) {
		filter->status++;
		filter->cache = (c & 0xff) << 16;
	} else if (n == 1) {
		filter->status++;
		filter->cache |= (c & 0xff) << 8;
	} else {
		filter->status &= ~0xff;
		n = (filter->status & 0xff00) >> 8;
		if (n > 72) {
			CK((*filter->output_function)(0x0d, filter->data));		/* CR */
			CK((*filter->output_function)(0x0a, filter->data));		/* LF */
			filter->status &= ~0xff00;
		}
		filter->status += 0x400;
		n = filter->cache | (c & 0xff);
		CK((*filter->output_function)(mb_base64_table[(n >> 18) & 0x3f], filter->data));
		CK((*filter->output_function)(mb_base64_table[(n >> 12) & 0x3f], filter->data));
		CK((*filter->output_function)(mb_base64_table[(n >> 6) & 0x3f], filter->data));
		CK((*filter->output_function)(mb_base64_table[n & 0x3f], filter->data));
	}

	return 0;
}

static int mb_filt_conv_base64enc_flush(mb_convert_filter *filter)
{
	int status, cache, len;

	status = filter->status & 0xff;
	cache = filter->cache;
	len = (filter->status & 0xff00) >> 8;
	filter->status &= ~0xffff;
	filter->cache = 0;
	/* flush fragments */
	if (status >= 1) {
		if (len > 72){
			CK((*filter->output_function)(0x0d, filter->data));		/* CR */
			CK((*filter->output_function)(0x0a, filter->data));		/* LF */
		}
		CK((*filter->output_function)(mb_base64_table[(cache >> 18) & 0x3f], filter->data));
		CK((*filter->output_function)(mb_base64_table[(cache >> 12) & 0x3f], filter->data));
		if (status == 1) {
			CK((*filter->output_function)(0x3d, filter->data));		/* '=' */
			CK((*filter->output_function)(0x3d, filter->data));		/* '=' */
		} else {
			CK((*filter->output_function)(mb_base64_table[(cache >> 6) & 0x3f], filter->data));
			CK((*filter->output_function)(0x3d, filter->data));		/* '=' */
		}
	}

	if (filter->flush_function) {
		(*filter->flush_function)(filter->data);
	}

	return 0;
}

/* BASE64 => any */
static int mb_filt_conv_base64dec(int c, mb_convert_filter *filter)
{
	int n;

	if (c == 0x0d || c == 0x0a || c == 0x20 || c == 0x09 || c == 0x3d) {	/* CR or LF or SPACE or HTAB or '=' */
		return 0;
	}

	n = 0;
	if (c >= 0x41 && c <= 0x5a) {		/* A - Z */
		n = c - 65;
	} else if (c >= 0x61 && c <= 0x7a) {	/* a - z */
		n = c - 71;
	} else if (c >= 0x30 && c <= 0x39) {	/* 0 - 9 */
		n = c + 4;
	} else if (c == 0x2b) {			/* '+' */
		n = 62;
	} else if (c == 0x2f) {			/* '/' */
		n = 63;
	} else {
		/* Invalid character - output a marker but continue */
		return 0;
	}
	n &= 0x3f;

	switch (filter->status) {
	case 0:
		filter->status = 1;
		filter->cache = n << 18;
		break;
	case 1:
		filter->status = 2;
		filter->cache |= n << 12;
		break;
	case 2:
		filter->status = 3;
		filter->cache |= n << 6;
		break;
	default:
		filter->status = 0;
		n |= filter->cache;
		CK((*filter->output_function)((n >> 16) & 0xff, filter->data));
		CK((*filter->output_function)((n >> 8) & 0xff, filter->data));
		CK((*filter->output_function)(n & 0xff, filter->data));
		break;
	}

	return 0;
}

static int mb_filt_conv_base64dec_flush(mb_convert_filter *filter)
{
	int status, cache;

	status = filter->status;
	cache = filter->cache;
	filter->status = 0;
	filter->cache = 0;
	/* flush fragments */
	if (status >= 2) {
		CK((*filter->output_function)((cache >> 16) & 0xff, filter->data));
		if (status >= 3) {
			CK((*filter->output_function)((cache >> 8) & 0xff, filter->data));
		}
	}

	if (filter->flush_function) {
		(*filter->flush_function)(filter->data);
	}

	return 0;
}

/* =============================================================================
 * Quoted-Printable encoding/decoding
 * ============================================================================= */

static int hex2code_map[] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
	-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

/* any => Quoted-Printable */
static int mb_filt_conv_qprintenc(int c, mb_convert_filter *filter)
{
	int s, n;

	switch (filter->status & 0xff) {
	case 0:
		filter->cache = c;
		filter->status++;
		break;
	default:
		s = filter->cache;
		filter->cache = c;
		n = (filter->status & 0xff00) >> 8;

		if (s == 0) {		/* null */
			CK((*filter->output_function)(s, filter->data));
			filter->status &= ~0xff00;
			break;
		}

		if (s == '\n' || (s == '\r' && c != '\n')) {	/* line feed */
			CK((*filter->output_function)('\r', filter->data));
			CK((*filter->output_function)('\n', filter->data));
			filter->status &= ~0xff00;
			break;
		} else if (s == 0x0d) {
			break;
		}

		if (n >= 72) {	/* soft line feed */
			CK((*filter->output_function)('=', filter->data));
			CK((*filter->output_function)('\r', filter->data));
			CK((*filter->output_function)('\n', filter->data));
			filter->status &= ~0xff00;
		}

		if (s <= 0 || s >= 0x80 || s == '=') { /* not ASCII or '=' */
			/* hex-octet */
			CK((*filter->output_function)('=', filter->data));
			n = (s >> 4) & 0xf;
			if (n < 10) {
				n += 48;		/* '0' */
			} else {
				n += 55;		/* 'A' - 10 */
			}
			CK((*filter->output_function)(n, filter->data));
			n = s & 0xf;
			if (n < 10) {
				n += 48;
			} else {
				n += 55;
			}
			CK((*filter->output_function)(n, filter->data));
			filter->status += 0x300;
		} else {
			CK((*filter->output_function)(s, filter->data));
			filter->status += 0x100;
		}
		break;
	}

	return 0;
}

static int mb_filt_conv_qprintenc_flush(mb_convert_filter *filter)
{
	/* flush filter cache */
	(*filter->filter_function)('\0', filter);
	filter->status &= ~0xffff;
	filter->cache = 0;

	if (filter->flush_function) {
		(*filter->flush_function)(filter->data);
	}

	return 0;
}

/* Quoted-Printable => any */
static int mb_filt_conv_qprintdec(int c, mb_convert_filter *filter)
{
	int n, m;

	switch (filter->status) {
	case 1:
		if (hex2code_map[c & 0xff] >= 0) {
			filter->cache = c;
			filter->status = 2;
		} else if (c == 0x0d) {	/* soft line feed */
			filter->status = 3;
		} else if (c == 0x0a) {	/* soft line feed */
			filter->status = 0;
		} else {
			CK((*filter->output_function)(0x3d, filter->data));		/* '=' */
			CK((*filter->output_function)(c, filter->data));
			filter->status = 0;
		}
		break;
	case 2:
		m = hex2code_map[c & 0xff];
		if (m < 0) {
			CK((*filter->output_function)(0x3d, filter->data));		/* '=' */
			CK((*filter->output_function)(filter->cache, filter->data));
			n = c;
		} else {
			n = hex2code_map[filter->cache] << 4 | m;
		}
		CK((*filter->output_function)(n, filter->data));
		filter->status = 0;
		break;
	case 3:
		if (c != 0x0a) {		/* LF */
			CK((*filter->output_function)(c, filter->data));
		}
		filter->status = 0;
		break;
	default:
		if (c == 0x3d) {		/* '=' */
			filter->status = 1;
		} else {
			CK((*filter->output_function)(c, filter->data));
		}
		break;
	}

	return 0;
}

static int mb_filt_conv_qprintdec_flush(mb_convert_filter *filter)
{
	int status, cache;

	status = filter->status;
	cache = filter->cache;
	filter->status = 0;
	filter->cache = 0;
	/* flush fragments */
	if (status == 1) {
		CK((*filter->output_function)(0x3d, filter->data));		/* '=' */
	} else if (status == 2) {
		CK((*filter->output_function)(0x3d, filter->data));		/* '=' */
		CK((*filter->output_function)(cache, filter->data));
	}

	if (filter->flush_function) {
		(*filter->flush_function)(filter->data);
	}

	return 0;
}

/* =============================================================================
 * Filter infrastructure
 * ============================================================================= */

static void mb_filt_conv_common_ctor(mb_convert_filter *filter)
{
	filter->status = 0;
	filter->cache = 0;
}

static const mb_convert_vtbl* mb_convert_filter_get_vtbl(const mb_encoding *from, const mb_encoding *to)
{
	if (from->no_encoding == mb_no_encoding_8bit && to->no_encoding == mb_no_encoding_base64) {
		return &vtbl_8bit_b64;
	} else if (from->no_encoding == mb_no_encoding_base64 && to->no_encoding == mb_no_encoding_8bit) {
		return &vtbl_b64_8bit;
	} else if (from->no_encoding == mb_no_encoding_8bit && to->no_encoding == mb_no_encoding_qprint) {
		return &vtbl_8bit_qprint;
	} else if (from->no_encoding == mb_no_encoding_qprint && to->no_encoding == mb_no_encoding_8bit) {
		return &vtbl_qprint_8bit;
	}
	return NULL;
}

static void mb_convert_filter_init(mb_convert_filter *filter, const mb_encoding *from, const mb_encoding *to,
	const mb_convert_vtbl *vtbl, mb_output_function_t output_function, mb_flush_function_t flush_function, void* data)
{
	filter->from = from;
	filter->to = to;
	filter->output_function = output_function;
	filter->flush_function = flush_function;
	filter->data = data;
	filter->filter_dtor = vtbl->filter_dtor;
	filter->filter_function = vtbl->filter_function;
	filter->filter_flush = vtbl->filter_flush;

	if (vtbl->filter_ctor) {
		(*vtbl->filter_ctor)(filter);
	}
}

mb_convert_filter* mb_convert_filter_new(const mb_encoding *from, const mb_encoding *to,
	mb_output_function_t output_function, mb_flush_function_t flush_function, void* data)
{
	const mb_convert_vtbl *vtbl = mb_convert_filter_get_vtbl(from, to);
	if (vtbl == NULL) {
		return NULL;
	}

	mb_convert_filter *filter = emalloc(sizeof(mb_convert_filter));
	mb_convert_filter_init(filter, from, to, vtbl, output_function, flush_function, data);
	return filter;
}

void mb_convert_filter_delete(mb_convert_filter *filter)
{
	if (filter->filter_dtor) {
		(*filter->filter_dtor)(filter);
	}
	efree(filter);
}

int mb_convert_filter_feed(int c, mb_convert_filter *filter)
{
	return (*filter->filter_function)(c, filter);
}

int mb_convert_filter_flush(mb_convert_filter *filter)
{
	(*filter->filter_flush)(filter);
	return 0;
}

/* =============================================================================
 * Encoding lookup functions
 * ============================================================================= */

const mb_encoding* mb_name2encoding(const char *name)
{
	if (name == NULL) {
		return NULL;
	}

	/* Case-insensitive comparison for encoding names */
	if (strcasecmp(name, "base64") == 0 || strcasecmp(name, "BASE64") == 0) {
		return &mb_encoding_base64;
	} else if (strcasecmp(name, "quoted-printable") == 0 || strcasecmp(name, "qprint") == 0) {
		return &mb_encoding_qprint;
	} else if (strcasecmp(name, "8bit") == 0) {
		return &mb_encoding_8bit;
	} else if (strcasecmp(name, "7bit") == 0) {
		return &mb_encoding_7bit;
	}

	return NULL;
}

const mb_encoding* mb_no2encoding(enum mb_no_encoding no_encoding)
{
	switch (no_encoding) {
		case mb_no_encoding_base64:
			return &mb_encoding_base64;
		case mb_no_encoding_qprint:
			return &mb_encoding_qprint;
		case mb_no_encoding_8bit:
			return &mb_encoding_8bit;
		case mb_no_encoding_7bit:
			return &mb_encoding_7bit;
		default:
			return NULL;
	}
}
