/*
   +----------------------------------------------------------------------+
   | PHP Version 4                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2004 The PHP Group                                |
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

#ifndef php_mailparse_rfc822_h
#define php_mailparse_rfc822_h

typedef struct _php_rfc822_token php_rfc822_token_t;
typedef struct _php_rfc822_tokenized php_rfc822_tokenized_t;
typedef struct _php_rfc822_address php_rfc822_address_t;
typedef struct _php_rfc822_addresses php_rfc822_addresses_t;

#define php_rfc822_token_is_atom(tok)	( (tok) == 0 || (tok) == '"' || (tok) == '(' )
struct _php_rfc822_token {
	int token;
	const char *value;
	int valuelen;
};

struct _php_rfc822_tokenized {
	php_rfc822_token_t *tokens;
	int ntokens;
	char *buffer;
};

struct _php_rfc822_address {
	char *name;
	char *address;
	int is_group;
};

struct _php_rfc822_addresses {
	php_rfc822_address_t *addrs;
	int naddrs;
};

PHP_MAILPARSE_API php_rfc822_tokenized_t *php_mailparse_rfc822_tokenize(const char *header, int report_errors TSRMLS_DC);
PHP_MAILPARSE_API void php_rfc822_tokenize_free(php_rfc822_tokenized_t *toks);

PHP_MAILPARSE_API php_rfc822_addresses_t *php_rfc822_parse_address_tokens(php_rfc822_tokenized_t *toks);
PHP_MAILPARSE_API void php_rfc822_free_addresses(php_rfc822_addresses_t *addrs);

#define PHP_RFC822_RECOMBINE_IGNORE_COMMENTS	1
#define PHP_RFC822_RECOMBINE_STRTOLOWER			2
#define PHP_RFC822_RECOMBINE_COMMENTS_TO_QUOTES	4
#define PHP_RFC822_RECOMBINE_SPACE_ATOMS		8
#define PHP_RFC822_RECOMBINE_INCLUDE_QUOTES		16
#define PHP_RFC822_RECOMBINE_COMMENTS_ONLY		32
PHP_MAILPARSE_API char *php_rfc822_recombine_tokens(php_rfc822_tokenized_t *toks, int first_token, int n_tokens, int flags);

void php_rfc822_print_tokens(php_rfc822_tokenized_t *toks);

#endif

