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

#ifndef PHP_MAILPARSE_ARGINFO_H
#define PHP_MAILPARSE_ARGINFO_H

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_test, 0, 0, 1)
        ZEND_ARG_INFO(0, header)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_mimemessage_construct, 0, 0, 2)
        ZEND_ARG_INFO(0, mode)
        ZEND_ARG_INFO(0, source)
ZEND_END_ARG_INFO()

#define arginfo_mailparse_mimemessage_remove arginfo_mailparse_void

#define arginfo_mailparse_mimemessage_add_child arginfo_mailparse_void

#define arginfo_mailparse_mimemessage_get_child_count arginfo_mailparse_void

#define arginfo_mailparse_mimemessage_get_parent arginfo_mailparse_void

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_mimemessage_get_child, 0, 0, 1)
        ZEND_ARG_INFO(0, item_to_find)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_mimemessage_extract_headers, 0, 0, 0)
        ZEND_ARG_INFO(0, mode)
        ZEND_ARG_INFO(0, arg)
ZEND_END_ARG_INFO()

#define arginfo_mailparse_mimemessage_extract_body arginfo_mailparse_mimemessage_extract_headers

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_mimemessage_extract_uue, 0, 0, 1)
        ZEND_ARG_INFO(0, index)
        ZEND_ARG_INFO(0, mode)
        ZEND_ARG_INFO(0, arg)
ZEND_END_ARG_INFO()

#define arginfo_mailparse_mimemessage_enum_uue arginfo_mailparse_void

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_fp, 0, 0, 1)
        ZEND_ARG_INFO(0, fp)
ZEND_END_ARG_INFO()

#define arginfo_mailparse_uudecode_all arginfo_mailparse_fp

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_rfc822_parse_addresses, 0, 0, 1)
        ZEND_ARG_INFO(0, addresses)
ZEND_END_ARG_INFO()

#define arginfo_mailparse_determine_best_xfer_encoding arginfo_mailparse_fp

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_stream_encode, 0, 0, 3)
        ZEND_ARG_INFO(0, source_fp)
        ZEND_ARG_INFO(0, dest_fp)
        ZEND_ARG_INFO(0, encoding)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_msg_parse, 0, 0, 2)
        ZEND_ARG_INFO(0, fp)
        ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_msg_parse_file, 0, 0, 1)
        ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()

#define arginfo_mailparse_msg_free arginfo_mailparse_fp

#define arginfo_mailparse_msg_create arginfo_mailparse_void

#define arginfo_mailparse_msg_get_structure arginfo_mailparse_fp

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_msg_extract_part, 0, 0, 2)
        ZEND_ARG_INFO(0, fp)
        ZEND_ARG_INFO(0, msgbody)
        ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_msg_extract_whole_part_file, 0, 0, 2)
        ZEND_ARG_INFO(0, fp)
        ZEND_ARG_INFO(0, filename)
        ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

#define arginfo_mailparse_msg_extract_part_file arginfo_mailparse_msg_extract_whole_part_file

#define arginfo_mailparse_msg_get_part_data arginfo_mailparse_fp

#define arginfo_mailparse_msg_get_part arginfo_mailparse_msg_parse

#endif
