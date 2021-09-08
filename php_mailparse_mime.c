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

#include "php.h"
#include "php_mailparse.h"
#include "php_mailparse_mime.h"
#include "php_mailparse_rfc822.h"

#define	MAXLEVELS	20
#define	MAXPARTS	300
#define IS_MIME_1(part)	(((part)->mime_version && strcmp("1.0", (part)->mime_version) == 0) || ((part)->parent))
#define CONTENT_TYPE_IS(part, contenttypevalue)	((part)->content_type && strcasecmp((part)->content_type->value, contenttypevalue) == 0)
#define CONTENT_TYPE_ISL(part, contenttypevalue, len)	((part)->content_type && strncasecmp((part)->content_type->value, contenttypevalue, len) == 0)
#define STR_FREE(ptr) if (ptr) { efree(ptr); }
#define mailparse_fetch_mimepart_resource(rfcvar, zvalarg) rfcvar = (php_mimepart *)zend_fetch_resource(Z_RES_P(zvalarg), php_mailparse_msg_name(), php_mailparse_le_mime_part())

static void php_mimeheader_free(struct php_mimeheader_with_attributes *attr)
{
	STR_FREE(attr->value);
	zval_ptr_dtor(&attr->attributes);
	efree(attr);
}

static struct php_mimeheader_with_attributes * php_mimeheader_alloc(char *value)
{
	struct php_mimeheader_with_attributes *attr;

	attr = ecalloc(1, sizeof(struct php_mimeheader_with_attributes));

	array_init(&attr->attributes);

	attr->value = estrdup(value);

	return attr;
}

void rfc2231_to_mime(smart_string* value_buf, char* value, int charset_p, int prevcharset_p)
{
	char *strp, *startofvalue = NULL;
	int quotes = 0;

	/* Process string, get positions and replace	*/
	/* Set to start of buffer*/
	if (charset_p) {

		/* Previous charset already set so only convert %nn to =nn*/
		if (prevcharset_p) {
			quotes=2;
		}

		strp = value;
		while (*strp) {

			/* Quote handling*/
			if (*strp == '\'') {
				if (quotes <= 1) {

					/* End of charset*/
					if (quotes == 0) {
						*strp=0;
					} else {
					 startofvalue = strp+1;
					}

					quotes++;
				}
			} else {
				/* Replace % with = - quoted printable*/
				if (*strp == '%' && quotes==2) {
					*strp = '=';
				}
			}
			strp++;
		}
	}

	/* If first encoded token*/
	if (charset_p && !prevcharset_p && startofvalue) {
		smart_string_appends(value_buf, "=?");
		smart_string_appends(value_buf, value);
		smart_string_appends(value_buf, "?Q?");
		smart_string_appends(value_buf, startofvalue);
	}

	/* If last encoded token*/
	if (prevcharset_p && !charset_p) {
		smart_string_appends(value_buf, "?=");
	}

	/* Append value*/
	if ((!charset_p || (prevcharset_p && charset_p)) && value) {
		smart_string_appends(value_buf, value);
	}
}

static struct php_mimeheader_with_attributes *php_mimeheader_alloc_from_tok(php_rfc822_tokenized_t *toks)
{
	struct php_mimeheader_with_attributes *attr;
	int i, first_semi, next_semi, comments_before_semi, netscape_bug = 0;
	char *name_buf = NULL;
	smart_string value_buf = {0};
	int is_rfc2231_name = 0;
	char *check_name;
	int charset_p, prevcharset_p = 0;
	int namechanged, currentencoded = 0;

	attr = ecalloc(1, sizeof(struct php_mimeheader_with_attributes));

	array_init(&attr->attributes);

/*  php_rfc822_print_tokens(toks); */

	/* look for optional ; which separates optional attributes from the main value */
	for (first_semi = 2; first_semi < toks->ntokens; first_semi++)
		if (toks->tokens[first_semi].token == ';') {
			break;
		}

	attr->value = php_rfc822_recombine_tokens(toks, 2, first_semi - 2,
			PHP_RFC822_RECOMBINE_STRTOLOWER | PHP_RFC822_RECOMBINE_IGNORE_COMMENTS);

	if (first_semi < toks->ntokens) {
		first_semi++;
	}

	/* Netscape Bug: Messenger sometimes omits the semi when wrapping the
     * the header.
	 * That means we have to be even more clever than the spec says that
	 * we need to :-/
	 * */

	while (first_semi < toks->ntokens) {
		/* find the next ; */
		comments_before_semi = 0;
		for (next_semi = first_semi; next_semi < toks->ntokens; next_semi++) {
			if (toks->tokens[next_semi].token == ';') {
				break;
			}
			if (toks->tokens[next_semi].token == '(') {
				comments_before_semi++;
			}
		}


		i = first_semi;
		if (i < next_semi)	{
			i++;

			/* ignore comments */
			while (i < next_semi && toks->tokens[i].token == '(')
				i++;

			if (i < next_semi && toks->tokens[i].token == '=') {
				char *name, *value;

				/* Here, next_semi --> "name" and i --> "=", so skip "=" sign */
				i++;

				/* count those tokens; we expect "token = token" (3 tokens); if there are
				 * more than that, then something is quite possibly wrong - Netscape Bug! */
				if (next_semi < toks->ntokens
						&& toks->tokens[next_semi].token != ';'
						&& next_semi - first_semi - comments_before_semi > 3) {
					next_semi = i + 1;
					netscape_bug = 1;
				}

				name = php_rfc822_recombine_tokens(toks, first_semi, 1,
						PHP_RFC822_RECOMBINE_STRTOLOWER|PHP_RFC822_RECOMBINE_IGNORE_COMMENTS);
				value = php_rfc822_recombine_tokens(toks, i, next_semi - i,
						PHP_RFC822_RECOMBINE_IGNORE_COMMENTS);

				/* support rfc2231 mime parameter value
				 *
				 * Parameter Value Continuations:
				 *
				 * Content-Type: message/external-body; access-type=URL;
				 *	URL*0="ftp://";
				 *	URL*1="cs.utk.edu/pub/moore/bulk-mailer/bulk-mailer.tar"
				 *
				 * is semantically identical to
				 *
				 * Content-Type: message/external-body; access-type=URL;
				 *	URL="ftp://cs.utk.edu/pub/moore/bulk-mailer/bulk-mailer.tar"
				 *
				 * Original rfc2231 support by IceWarp Ltd. <info@icewarp.com>
				 */
				check_name = strchr(name, '*');
				if (check_name) {
				  currentencoded = 1;

					/* Is last char * - charset encoding */
					charset_p = *(name+strlen(name)-1) == '*';

					/* Leave only attribute name without * */
					*check_name = 0;

					/* New item or continuous */
					if (NULL == name_buf) {
						namechanged = 0;
						name_buf = name;
					} else {
						namechanged = (strcmp(name_buf, name) != 0);
						if (!namechanged) {
							efree(name);
							name = 0;
						}
					}

					/* Check if name changed*/
					if (!namechanged) {

						/* Append string to buffer - check if to be encoded...	*/
						rfc2231_to_mime(&value_buf, value, charset_p, prevcharset_p);

						/* Mark previous */
						prevcharset_p = charset_p;
					}

					is_rfc2231_name = 1;
				}

				/* Last item was encoded	*/
				if (1 == is_rfc2231_name) {
					/* Name not null and name differs with new name*/
					if (name && strcmp(name_buf, name) != 0) {
						/* Finalize packet */
						rfc2231_to_mime(&value_buf, NULL, 0, prevcharset_p);

						add_assoc_stringl(&attr->attributes, name_buf, value_buf.c, value_buf.len);
						efree(name_buf);
						smart_string_free(&value_buf);

						prevcharset_p = 0;
						is_rfc2231_name = 0;
						name_buf = NULL;

						/* New non encoded name*/
						if (!currentencoded) {
							/* Add string*/
							add_assoc_string(&attr->attributes, name, value);
							efree(name);
						} else {		/* Encoded name changed*/
							if (namechanged) {
								/* Append string to buffer - check if to be encoded...	*/
								rfc2231_to_mime(&value_buf, value, charset_p, prevcharset_p);

								/* Mark */
								is_rfc2231_name = 1;
								name_buf = name;
								prevcharset_p = charset_p;
							}
						}

						namechanged = 0;
					}
				} else {
					add_assoc_string(&attr->attributes, name, value);
					efree(name);
				}

				efree(value);
			}
		}

		if (next_semi < toks->ntokens && !netscape_bug) {
			next_semi++;
		}

		first_semi = next_semi;
		netscape_bug = 0;
	}

	if (1 == is_rfc2231_name) {
		/* Finalize packet */
		rfc2231_to_mime(&value_buf, NULL, 0, prevcharset_p);

		add_assoc_stringl(&attr->attributes, name_buf, value_buf.c, value_buf.len);
		efree(name_buf);
		smart_string_free(&value_buf);
	}


	return attr;
}

static void php_mimepart_free_child(zval *childpart_z)
{
	php_mimepart *part = (php_mimepart *)zend_fetch_resource(Z_RES_P(childpart_z), php_mailparse_msg_name(), php_mailparse_le_mime_part());
	if (part != NULL) {
		part->parent = NULL;
		zend_list_delete(part->rsrc);
	}
}

PHP_MAILPARSE_API php_mimepart *php_mimepart_alloc()
{
	php_mimepart *part = ecalloc(1, sizeof(php_mimepart));

	part->part_index = 1;

	zend_hash_init(&part->children, 0, NULL, (dtor_func_t)php_mimepart_free_child, 0);

	array_init(&part->headerhash);

	ZVAL_NULL(&part->source.zval);

	/* begin in header parsing mode */
	part->parsedata.in_header = 1;
	part->rsrc = zend_register_resource(part, php_mailparse_le_mime_part());
	return part;
}


PHP_MAILPARSE_API void php_mimepart_free(php_mimepart *part)
{
	/* free contained parts */

	zend_hash_destroy(&part->children);

	STR_FREE(part->mime_version);
	STR_FREE(part->content_transfer_encoding);
	STR_FREE(part->charset);
	STR_FREE(part->boundary);
	STR_FREE(part->content_base);
	STR_FREE(part->content_location);

	if (part->content_type) {
		php_mimeheader_free(part->content_type);
		part->content_type = NULL;
	}
	if (part->content_disposition) {
		php_mimeheader_free(part->content_disposition);
		part->content_disposition = NULL;
	}

	smart_string_free(&part->parsedata.workbuf);
	smart_string_free(&part->parsedata.headerbuf);

	zval_ptr_dtor(&part->source.zval);

	zval_ptr_dtor(&part->headerhash);

	efree(part);
}

static void php_mimepart_update_positions(php_mimepart *part, size_t newendpos, size_t newbodyend, size_t deltanlines)
{
	while(part) {
		part->endpos = newendpos;
		part->bodyend = newbodyend;
		part->nlines += deltanlines;
		if (!part->parsedata.in_header) {
			part->nbodylines += deltanlines;
		}
		part = part->parent;
	}
}

PHP_MAILPARSE_API char *php_mimepart_attribute_get(struct php_mimeheader_with_attributes *attr, char *attrname)
{
	zval *attrval;
	zend_string *hash_key;

	hash_key = zend_string_init(attrname, strlen(attrname), 0);
        attrval = zend_hash_find(Z_ARRVAL_P(&attr->attributes), hash_key);
	zend_string_release(hash_key);

	if (attrval != NULL) {
	      return Z_STRVAL_P(attrval);
	}
	return NULL;
}

#define STR_SET_REPLACE(ptr, newval)	do { STR_FREE(ptr); ptr = estrdup(newval); } while(0)

static int php_mimepart_process_header(php_mimepart *part)
{
	php_rfc822_tokenized_t *toks;
	char *header_key, *header_val, *header_val_stripped;
	zval *zheaderval;
	zend_string *header_zstring;

	if (part->parsedata.headerbuf.len == 0) {
		return SUCCESS;
	}

	smart_string_0(&part->parsedata.headerbuf);

	/* parse the header line */
	toks = php_mailparse_rfc822_tokenize((const char*)part->parsedata.headerbuf.c, 0);

	/* valid headers consist of at least three tokens, with the first being a string and the
	 * second token being a ':' */
	if (toks->ntokens < 2 || toks->tokens[0].token != 0 || toks->tokens[1].token != ':') {
		part->parsedata.headerbuf.len = 0;

		php_rfc822_tokenize_free(toks);
		return FAILURE;
	}

	/* get a lower-case version of the first token */
	header_key = php_rfc822_recombine_tokens(toks, 0, 1, PHP_RFC822_RECOMBINE_IGNORE_COMMENTS|PHP_RFC822_RECOMBINE_STRTOLOWER);

	header_val = strchr(part->parsedata.headerbuf.c, ':');
	header_val_stripped = php_rfc822_recombine_tokens(toks, 2, toks->ntokens-2, PHP_RFC822_RECOMBINE_IGNORE_COMMENTS|PHP_RFC822_RECOMBINE_STRTOLOWER);

	if (header_val) {
		header_val++;
		while (isspace(*header_val))
			header_val++;

		/* add the header to the hash.
		 * join multiple To: or Cc: lines together */
		header_zstring = zend_string_init(header_key, strlen(header_key), 0);
		if ((strcmp(header_key, "to") == 0 || strcmp(header_key, "cc") == 0) && (zheaderval = zend_hash_find(Z_ARRVAL_P(&part->headerhash), header_zstring)) != NULL) {
			int newlen;
			char *newstr;

			newlen = strlen(header_val) + Z_STRLEN_P(zheaderval) + 3;
			newstr = emalloc(newlen);

			strcpy(newstr, Z_STRVAL_P(zheaderval));
			strcat(newstr, ", ");
			strcat(newstr, header_val);
			add_assoc_string(&part->headerhash, header_key, newstr);
			efree(newstr);
		} else {
			if((zheaderval = zend_hash_find(Z_ARRVAL_P(&part->headerhash), header_zstring)) != NULL) {
			      if(Z_TYPE_P(zheaderval) == IS_ARRAY) {
          add_next_index_string(zheaderval, header_val);
        } else {
          /* Create a nested array if there is more than one of the same header */
          zval zarr;
          array_init(&zarr);
          Z_ADDREF_P(zheaderval);

          add_next_index_zval(&zarr, zheaderval);
          add_next_index_string(&zarr, header_val);
          add_assoc_zval(&part->headerhash, header_key, &zarr);
        }
      } else {
        add_assoc_string(&part->headerhash, header_key, header_val);
      }
		}
		zend_string_release(header_zstring);
		/* if it is useful, keep a pointer to it in the mime part */
		if (strcmp(header_key, "mime-version") == 0) {
			STR_SET_REPLACE(part->mime_version, header_val_stripped);
		}

		if (strcmp(header_key, "content-location") == 0) {
			STR_FREE(part->content_location);
			part->content_location = php_rfc822_recombine_tokens(toks, 2, toks->ntokens-2, PHP_RFC822_RECOMBINE_IGNORE_COMMENTS);
		}
		if (strcmp(header_key, "content-base") == 0) {
			STR_FREE(part->content_base);
			part->content_base = php_rfc822_recombine_tokens(toks, 2, toks->ntokens-2, PHP_RFC822_RECOMBINE_IGNORE_COMMENTS);
		}

		if (strcmp(header_key, "content-transfer-encoding") == 0) {
			STR_SET_REPLACE(part->content_transfer_encoding, header_val_stripped);
		}
		if (strcmp(header_key, "content-type") == 0) {
			char *charset, *boundary;

			if (part->content_type) {
				php_mimeheader_free(part->content_type);
				part->content_type = NULL;
			}

			part->content_type = php_mimeheader_alloc_from_tok(toks);

			boundary = php_mimepart_attribute_get(part->content_type, "boundary");
			if (boundary) {
				part->boundary = estrdup(boundary);
			}

			charset = php_mimepart_attribute_get(part->content_type, "charset");
			if (charset) {
				STR_SET_REPLACE(part->charset, charset);
			}
		}
		if (strcmp(header_key, "content-disposition") == 0) {
			if (part->content_disposition) {
				php_mimeheader_free(part->content_disposition);
				part->content_disposition = NULL;
			}
			part->content_disposition = php_mimeheader_alloc_from_tok(toks);
		}

	}
	STR_FREE(header_key);
	STR_FREE(header_val_stripped);

	php_rfc822_tokenize_free(toks);

	/* zero the buffer size */
	part->parsedata.headerbuf.len = 0;
	return SUCCESS;
}

static php_mimepart *alloc_new_child_part(php_mimepart *parentpart, size_t startpos, int inherit)
{
	php_mimepart *child = php_mimepart_alloc();
	zval child_z;

	parentpart->parsedata.lastpart = child;
	child->parent = parentpart;

	child->source.kind = parentpart->source.kind;
	if (parentpart->source.kind != mpNONE) {
		child->source.zval = parentpart->source.zval;
		zval_copy_ctor(&child->source.zval);
	}

        ZVAL_RES(&child_z, child->rsrc);
	zend_hash_next_index_insert(&parentpart->children, &child_z);
	child->startpos = child->endpos = child->bodystart = child->bodyend = startpos;

	if (inherit) {
		if (parentpart->content_transfer_encoding) {
			child->content_transfer_encoding = estrdup(parentpart->content_transfer_encoding);
		}
		if (parentpart->charset) {
			child->charset = estrdup(parentpart->charset);
		}
	}

	return child;
}

PHP_MAILPARSE_API void php_mimepart_get_offsets(php_mimepart *part, off_t *start, off_t *end, off_t *start_body, int *nlines, int *nbodylines)
{
	*start = part->startpos;
	*end = part->endpos;
	*nlines = part->nlines;
	*nbodylines = part->nbodylines;
	*start_body = part->bodystart;

	/* Adjust for newlines in mime parts */
	if (part->parent) {
		*end = part->bodyend;
		if (*nlines) {
			--*nlines;
		}
		if (*nbodylines) {
			--*nbodylines;
		}
	}
}

static int php_mimepart_process_line(php_mimepart *workpart)
{
	size_t origcount, linelen;
	char *c;

	/* sanity check */
	if (zend_hash_num_elements(&workpart->children) > MAXPARTS) {
		php_error_docref(NULL, E_WARNING, "MIME message too complex");
		return FAILURE;
	}

	c = workpart->parsedata.workbuf.c;
	smart_string_0(&workpart->parsedata.workbuf);

	/* strip trailing \r\n -- we always have a trailing \n */
	origcount = workpart->parsedata.workbuf.len;
	linelen = origcount - 1;
	if (linelen && c[linelen-1] == '\r') {
		--linelen;
	}

	/* Discover which part we were last working on */
	while (workpart->parsedata.lastpart) {
		size_t bound_len;
		php_mimepart *lastpart = workpart->parsedata.lastpart;

		if (lastpart->parsedata.completed) {
			php_mimepart_update_positions(workpart, workpart->endpos + origcount, workpart->endpos + origcount, 1);
			return SUCCESS;
		}
		if (workpart->boundary == NULL || workpart->parsedata.in_header) {
			workpart = lastpart;
			continue;
		}
		bound_len = strlen(workpart->boundary);

		/* Look for a boundary */
		if (c[0] == '-' && c[1] == '-' && linelen >= 2+bound_len && strncasecmp(workpart->boundary, c+2, bound_len) == 0) {
			php_mimepart *newpart;

			/* is it the final boundary ? */
			if (linelen >= 4 + bound_len && strncmp(c+2+bound_len, "--", 2) == 0) {
				lastpart->parsedata.completed = 1;
				php_mimepart_update_positions(workpart, workpart->endpos + origcount, workpart->endpos + origcount, 1);
				return SUCCESS;
			}

			newpart = alloc_new_child_part(workpart, workpart->endpos + origcount, 1);
			php_mimepart_update_positions(workpart, workpart->endpos + origcount, workpart->endpos + linelen, 1);
			newpart->mime_version = estrdup(workpart->mime_version);
			newpart->parsedata.in_header = 1;
			return SUCCESS;
		}
		workpart = lastpart;
	}

	if (!workpart->parsedata.in_header) {
		if (!workpart->parsedata.completed && !workpart->parsedata.lastpart) {
			/* update the body/part end positions.
			 * For multipart messages, the final newline belongs to the boundary.
			 * Otherwise it belongs to the body
			 * */
			if (workpart->parent && CONTENT_TYPE_ISL(workpart->parent, "multipart/", 10)) {
				php_mimepart_update_positions(workpart, workpart->endpos + origcount, workpart->endpos + linelen, 1);
			} else {
				php_mimepart_update_positions(workpart, workpart->endpos + origcount, workpart->endpos + origcount, 1);
			}
		}
	} else {

		if (linelen > 0) {

			php_mimepart_update_positions(workpart, workpart->endpos + origcount, workpart->endpos + linelen, 1);

			if (*c == ' ' || *c == '\t') {
			/* This doesn't technically confirm to rfc2822, as we're replacing \t with \s, but this seems to fix
			 * cases where clients incorrectly fold by inserting a \t character.
			 */
				smart_string_appendl(&workpart->parsedata.headerbuf, " ", 1);
				c++; linelen--;
			} else {
				php_mimepart_process_header(workpart);
			}
			/* save header for possible continuation */
			smart_string_appendl(&workpart->parsedata.headerbuf, c, linelen);

		} else {
			/* end of headers */
			php_mimepart_process_header(workpart);

			/* start of body */
			workpart->parsedata.in_header = 0;
			workpart->bodystart = workpart->endpos + origcount;
			php_mimepart_update_positions(workpart, workpart->bodystart, workpart->bodystart, 1);
			--workpart->nbodylines;

			/* some broken mailers include the content-type header but not a mime-version header.
			 * some others may use a MIME version other than 1.0.
			 * Let's relax and pretend they said they were mime 1.0 compatible */
			if (!IS_MIME_1(workpart) && workpart->content_type != NULL) {
				if (workpart->mime_version != NULL) {
					efree(workpart->mime_version);
				}
				workpart->mime_version = estrdup("1.0");
			}

			if (!IS_MIME_1(workpart)) {
				/* if we don't understand the MIME version, discard the content-type and
				 * boundary */
				if (workpart->content_disposition) {
					php_mimeheader_free(workpart->content_disposition);
					workpart->content_disposition = NULL;
				}
				if (workpart->boundary) {
					efree(workpart->boundary);
					workpart->boundary = NULL;
				}
				if (workpart->content_type) {
					php_mimeheader_free(workpart->content_type);
					workpart->content_type = NULL;
				}
				workpart->content_type = php_mimeheader_alloc("text/plain");
			}
			/* if there is no content type, default to text/plain, but use multipart/digest when in
			 * a multipart/rfc822 message */
			if (IS_MIME_1(workpart) && workpart->content_type == NULL) {
				char *def_type = "text/plain";

				if (workpart->parent && CONTENT_TYPE_IS(workpart->parent, "multipart/digest")) {
					def_type = "message/rfc822";
				}

				workpart->content_type = php_mimeheader_alloc(def_type);
			}

			/* if no charset had previously been set, either through inheritance or by an
			 * explicit content-type header, default to us-ascii */
			if (workpart->charset == NULL) {
				workpart->charset = estrdup(MAILPARSEG(def_charset));
			}

			if (CONTENT_TYPE_IS(workpart, "message/rfc822")) {
				workpart = alloc_new_child_part(workpart, workpart->bodystart, 0);
				workpart->parsedata.in_header = 1;
				return SUCCESS;

			}

			/* create a section for the preamble that precedes the first boundary */
			if (workpart->boundary) {
				workpart = alloc_new_child_part(workpart, workpart->bodystart, 1);
				workpart->parsedata.in_header = 0;
				workpart->parsedata.is_dummy = 1;
				return SUCCESS;
			}

			return SUCCESS;
		}

	}

	return SUCCESS;
}

PHP_MAILPARSE_API int php_mimepart_parse(php_mimepart *part, const char *buf, size_t bufsize)
{
	size_t len;

	while(bufsize > 0) {
		/* look for EOL */
		for (len = 0; len < bufsize; len++)
			if (buf[len] == '\n') {
				break;
			}
		if (len < bufsize && buf[len] == '\n') {
			++len;
			smart_string_appendl(&part->parsedata.workbuf, buf, len);
			if (php_mimepart_process_line(part) == FAILURE) {
				/* php_mimepart_process_line() only returns FAILURE in case the count of children
				 * have exceeded MAXPARTS and doing so at the very begining, without doing any work.
				 * It'd do this for all of the following lines, since the exceeded state won't change.
				 * As no additional work have been done since the last php_mimepart_process_line() call,
				 * it is safe to break the loop now not caring about the rest of the code.
				 *
				 * Known issues:
				 *  - some callers aren't obeying the returned value, but that's in the mailmessage
				 *    object which is not documented and seemingly otdated/unfinished anyway
				 */
				return FAILURE;
			};
			part->parsedata.workbuf.len = 0;
		} else {
			smart_string_appendl(&part->parsedata.workbuf, buf, len);
		}

		buf += len;
		bufsize -= len;
	}
	return SUCCESS;
}

static int enum_parts_recurse(php_mimepart_enumerator *top, php_mimepart_enumerator **child,
		php_mimepart *part, mimepart_enumerator_func callback, void *ptr)
{
	php_mimepart_enumerator next;
	php_mimepart *childpart;
	zval *childpart_z;
	HashPosition pos;

	*child = NULL;
	if (FAILURE == (*callback)(part, top, ptr)) {
		return FAILURE;
	}

	*child = &next;
	next.id = 1;

	if (CONTENT_TYPE_ISL(part, "multipart/", 10)) {
		next.id = 0;
	}

	zend_hash_internal_pointer_reset_ex(&part->children, &pos);
	while ((childpart_z = zend_hash_get_current_data_ex(&part->children, &pos)) != NULL) {
	      mailparse_fetch_mimepart_resource(childpart, childpart_z);
		if (next.id) {
			if (FAILURE == enum_parts_recurse(top, &next.next, childpart, callback, ptr)) {
				return FAILURE;
			}
		}
		next.id++;
		zend_hash_move_forward_ex(&part->children, &pos);
	}
	return SUCCESS;
}

PHP_MAILPARSE_API void php_mimepart_enum_parts(php_mimepart *part, mimepart_enumerator_func callback, void *ptr)
{
	php_mimepart_enumerator top;
	top.id = 1;

	enum_parts_recurse(&top, &top.next, part, callback, ptr);
}

PHP_MAILPARSE_API void php_mimepart_enum_child_parts(php_mimepart *part, mimepart_child_enumerator_func callback, void *ptr)
{
	HashPosition pos;
	php_mimepart *childpart;
	zval *childpart_z;

	int index = 0;

	zend_hash_internal_pointer_reset_ex(&part->children, &pos);
	while ((childpart_z = zend_hash_get_current_data_ex(&part->children, &pos)) != NULL) {
		mailparse_fetch_mimepart_resource(childpart, childpart_z);
		if (FAILURE == (*callback)(part, childpart, index, ptr)) {
			return;
		}

		zend_hash_move_forward_ex(&part->children, &pos);
		index++;
	}
}

struct find_part_struct {
	const char *searchfor;
	php_mimepart *foundpart;
};

static int find_part_callback(php_mimepart *part, php_mimepart_enumerator *id, void *ptr)
{
	struct find_part_struct *find = ptr;
	const unsigned char *num = (const unsigned char*)find->searchfor;
	unsigned int n;

	while (id)	{
		if (!isdigit((int)*num)) {
			return SUCCESS;
		}
		/* convert from decimal to int */
		n = 0;
		while (isdigit((int)*num)) {
			n = (n * 10) + (*num++ - '0');
		}
		if (*num) {
			if (*num != '.') {
				return SUCCESS;
			}
			num++;
		}
		if (n != (unsigned int)id->id) {
			return SUCCESS;
		}
		id = id->next;
	}
	if (*num == 0) {
		find->foundpart = part;
	}

	return SUCCESS;
}

PHP_MAILPARSE_API php_mimepart *php_mimepart_find_by_name(php_mimepart *parent, const char *name)
{
	struct find_part_struct find;

	find.searchfor = name;
	find.foundpart = NULL;
	php_mimepart_enum_parts(parent, find_part_callback, &find);
	return find.foundpart;
}

PHP_MAILPARSE_API php_mimepart *php_mimepart_find_child_by_position(php_mimepart *parent, int position)
{
	HashPosition pos;
	php_mimepart *childpart = NULL;
	zval *childpart_z;

	zend_hash_internal_pointer_reset_ex(&parent->children, &pos);
	while(position-- > 0)
		if (FAILURE == zend_hash_move_forward_ex(&parent->children, &pos)) {
			return NULL;
		}

	if ((childpart_z = zend_hash_get_current_data_ex(&parent->children, &pos)) != NULL) {
		mailparse_fetch_mimepart_resource(childpart, childpart_z);
		if(childpart) {
			return childpart;
		}
	}

	return NULL;
}

static int filter_into_work_buffer(int c, void *dat)
{
	php_mimepart *part = dat;

	smart_string_appendc(&part->parsedata.workbuf, c);

	if (part->parsedata.workbuf.len >= 4096) {

		part->extract_func(part, part->extract_context, part->parsedata.workbuf.c, part->parsedata.workbuf.len);
		part->parsedata.workbuf.len = 0;
	}

	return c;
}

PHP_MAILPARSE_API void php_mimepart_decoder_prepare(php_mimepart *part, int do_decode, php_mimepart_extract_func_t decoder, void *ptr)
{
	enum mbfl_no_encoding from = mbfl_no_encoding_8bit;

	if (do_decode && part->content_transfer_encoding) {
		from = mbfl_name2no_encoding(part->content_transfer_encoding);
		if (from == mbfl_no_encoding_invalid) {
			if (strcasecmp("binary", part->content_transfer_encoding) != 0) {
				zend_error(E_WARNING, "%s(): mbstring doesn't know how to decode %s transfer encoding!",
						get_active_function_name(),
						part->content_transfer_encoding);
			}
			from = mbfl_no_encoding_8bit;
		}
	}

	part->extract_func = decoder;
	part->extract_context = ptr;
	part->parsedata.workbuf.len = 0;

	if (do_decode) {
		if (from == mbfl_no_encoding_8bit || from == mbfl_no_encoding_7bit) {
			part->extract_filter = NULL;
		} else {
#if PHP_VERSION_ID >= 70300
			part->extract_filter = mbfl_convert_filter_new(
					mbfl_no2encoding(from), mbfl_no2encoding(mbfl_no_encoding_8bit),
					filter_into_work_buffer,
					NULL,
					part
					);
#else
			part->extract_filter = mbfl_convert_filter_new(
					from, mbfl_no_encoding_8bit,
					filter_into_work_buffer,
					NULL,
					part
					);
#endif
		}
	}

}

PHP_MAILPARSE_API void php_mimepart_decoder_finish(php_mimepart *part)
{
	if (part->extract_filter) {
		mbfl_convert_filter_flush(part->extract_filter);
		mbfl_convert_filter_delete(part->extract_filter);
	}
	if (part->extract_func && part->parsedata.workbuf.len > 0) {
		part->extract_func(part, part->extract_context, part->parsedata.workbuf.c, part->parsedata.workbuf.len);
		part->parsedata.workbuf.len = 0;
	}
}

PHP_MAILPARSE_API int php_mimepart_decoder_feed(php_mimepart *part, const char *buf, size_t bufsize)
{
	if (buf && bufsize) {
		size_t i;

		if (part->extract_filter) {
			for (i = 0; i < bufsize; i++) {
				if (mbfl_convert_filter_feed(buf[i], part->extract_filter) < 0) {
					zend_error(E_WARNING, "%s() - filter conversion failed. Input message is probably incorrectly encoded\n",
							get_active_function_name());
					return -1;
				}
			}
		} else {
			return part->extract_func(part, part->extract_context, buf, bufsize);
		}
	}
	return 0;
}

PHP_MAILPARSE_API void php_mimepart_remove_from_parent(php_mimepart *part)
{
	php_mimepart *parent = part->parent;
	HashPosition pos;
	php_mimepart *childpart;
	zval *childpart_z;

	if (parent == NULL) {
		return;
	}

	part->parent = NULL;

	zend_hash_internal_pointer_reset_ex(&parent->children, &pos);
	while((childpart_z = zend_hash_get_current_data_ex(&parent->children, &pos)) != NULL) {
		if ((childpart_z = zend_hash_get_current_data_ex(&parent->children, &pos)) != NULL) {
			mailparse_fetch_mimepart_resource(childpart, childpart_z);
			if (childpart == part) {
				zend_ulong h;
				zend_hash_get_current_key_ex(&parent->children, NULL, &h, &pos);
				zend_hash_index_del(&parent->children, h);
				break;
			}
		}
		zend_hash_move_forward_ex(&parent->children, &pos);
	}
}

PHP_MAILPARSE_API void php_mimepart_add_child(php_mimepart *part, php_mimepart *child)
{

}
