/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: cbf00f7423b1b7cd1f2625bda1b4fd6851ac7e13 */

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_test, 0, 0, 1)
	ZEND_ARG_INFO(0, header)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_uudecode_all, 0, 0, 1)
	ZEND_ARG_INFO(0, fp)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_rfc822_parse_addresses, 0, 0, 1)
	ZEND_ARG_INFO(0, addresses)
ZEND_END_ARG_INFO()

#define arginfo_mailparse_determine_best_xfer_encoding arginfo_mailparse_uudecode_all

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

#define arginfo_mailparse_msg_free arginfo_mailparse_uudecode_all

ZEND_BEGIN_ARG_INFO_EX(arginfo_mailparse_msg_create, 0, 0, 0)
ZEND_END_ARG_INFO()

#define arginfo_mailparse_msg_get_structure arginfo_mailparse_uudecode_all

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

#define arginfo_mailparse_msg_get_part_data arginfo_mailparse_uudecode_all

#define arginfo_mailparse_msg_get_part arginfo_mailparse_msg_parse

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_mimemessage___construct, 0, 0, 2)
	ZEND_ARG_INFO(0, mode)
	ZEND_ARG_INFO(0, source)
ZEND_END_ARG_INFO()

#define arginfo_class_mimemessage_remove arginfo_mailparse_msg_create

#define arginfo_class_mimemessage_add_child arginfo_mailparse_msg_create

#define arginfo_class_mimemessage_get_child_count arginfo_mailparse_msg_create

#define arginfo_class_mimemessage_get_parent arginfo_mailparse_msg_create

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_mimemessage_get_child, 0, 0, 1)
	ZEND_ARG_INFO(0, item_to_find)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_mimemessage_extract_headers, 0, 0, 0)
	ZEND_ARG_INFO(0, mode)
	ZEND_ARG_INFO(0, arg)
ZEND_END_ARG_INFO()

#define arginfo_class_mimemessage_extract_body arginfo_class_mimemessage_extract_headers

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_mimemessage_extract_uue, 0, 0, 1)
	ZEND_ARG_INFO(0, index)
	ZEND_ARG_INFO(0, mode)
	ZEND_ARG_INFO(0, arg)
ZEND_END_ARG_INFO()

#define arginfo_class_mimemessage_enum_uue arginfo_mailparse_msg_create

ZEND_FUNCTION(mailparse_test);
ZEND_FUNCTION(mailparse_uudecode_all);
ZEND_FUNCTION(mailparse_rfc822_parse_addresses);
ZEND_FUNCTION(mailparse_determine_best_xfer_encoding);
ZEND_FUNCTION(mailparse_stream_encode);
ZEND_FUNCTION(mailparse_msg_parse);
ZEND_FUNCTION(mailparse_msg_parse_file);
ZEND_FUNCTION(mailparse_msg_free);
ZEND_FUNCTION(mailparse_msg_create);
ZEND_FUNCTION(mailparse_msg_get_structure);
ZEND_FUNCTION(mailparse_msg_extract_part);
ZEND_FUNCTION(mailparse_msg_extract_whole_part_file);
ZEND_FUNCTION(mailparse_msg_extract_part_file);
ZEND_FUNCTION(mailparse_msg_get_part_data);
ZEND_FUNCTION(mailparse_msg_get_part);
ZEND_METHOD(mimemessage, __construct);
ZEND_METHOD(mimemessage, remove);
ZEND_METHOD(mimemessage, add_child);
ZEND_METHOD(mimemessage, get_child_count);
ZEND_METHOD(mimemessage, get_parent);
ZEND_METHOD(mimemessage, get_child);
ZEND_METHOD(mimemessage, extract_headers);
ZEND_METHOD(mimemessage, extract_body);
ZEND_METHOD(mimemessage, extract_uue);
ZEND_METHOD(mimemessage, enum_uue);

static const zend_function_entry ext_functions[] = {
	ZEND_FE(mailparse_test, arginfo_mailparse_test)
	ZEND_FE(mailparse_uudecode_all, arginfo_mailparse_uudecode_all)
	ZEND_FE(mailparse_rfc822_parse_addresses, arginfo_mailparse_rfc822_parse_addresses)
	ZEND_FE(mailparse_determine_best_xfer_encoding, arginfo_mailparse_determine_best_xfer_encoding)
	ZEND_FE(mailparse_stream_encode, arginfo_mailparse_stream_encode)
	ZEND_FE(mailparse_msg_parse, arginfo_mailparse_msg_parse)
	ZEND_FE(mailparse_msg_parse_file, arginfo_mailparse_msg_parse_file)
	ZEND_FE(mailparse_msg_free, arginfo_mailparse_msg_free)
	ZEND_FE(mailparse_msg_create, arginfo_mailparse_msg_create)
	ZEND_FE(mailparse_msg_get_structure, arginfo_mailparse_msg_get_structure)
	ZEND_FE(mailparse_msg_extract_part, arginfo_mailparse_msg_extract_part)
	ZEND_FE(mailparse_msg_extract_whole_part_file, arginfo_mailparse_msg_extract_whole_part_file)
	ZEND_FE(mailparse_msg_extract_part_file, arginfo_mailparse_msg_extract_part_file)
	ZEND_FE(mailparse_msg_get_part_data, arginfo_mailparse_msg_get_part_data)
	ZEND_FE(mailparse_msg_get_part, arginfo_mailparse_msg_get_part)
	ZEND_FE_END
};

static const zend_function_entry class_mimemessage_methods[] = {
	ZEND_ME(mimemessage, __construct, arginfo_class_mimemessage___construct, ZEND_ACC_PUBLIC)
	ZEND_ME(mimemessage, remove, arginfo_class_mimemessage_remove, ZEND_ACC_PUBLIC)
	ZEND_ME(mimemessage, add_child, arginfo_class_mimemessage_add_child, ZEND_ACC_PUBLIC)
	ZEND_ME(mimemessage, get_child_count, arginfo_class_mimemessage_get_child_count, ZEND_ACC_PUBLIC)
	ZEND_ME(mimemessage, get_parent, arginfo_class_mimemessage_get_parent, ZEND_ACC_PUBLIC)
	ZEND_ME(mimemessage, get_child, arginfo_class_mimemessage_get_child, ZEND_ACC_PUBLIC)
	ZEND_ME(mimemessage, extract_headers, arginfo_class_mimemessage_extract_headers, ZEND_ACC_PUBLIC)
	ZEND_ME(mimemessage, extract_body, arginfo_class_mimemessage_extract_body, ZEND_ACC_PUBLIC)
	ZEND_ME(mimemessage, extract_uue, arginfo_class_mimemessage_extract_uue, ZEND_ACC_PUBLIC)
	ZEND_ME(mimemessage, enum_uue, arginfo_class_mimemessage_enum_uue, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static void register_mailparse_symbols(int module_number)
{
	REGISTER_LONG_CONSTANT("MAILPARSE_EXTRACT_OUTPUT", MAILPARSE_EXTRACT_OUTPUT, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("MAILPARSE_EXTRACT_STREAM", MAILPARSE_EXTRACT_STREAM, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("MAILPARSE_EXTRACT_RETURN", MAILPARSE_EXTRACT_RETURN, CONST_PERSISTENT | CONST_CS);
}

static zend_class_entry *register_class_mimemessage(void)
{
	zend_class_entry ce, *class_entry;

	INIT_CLASS_ENTRY(ce, "mimemessage", class_mimemessage_methods);
#if (PHP_VERSION_ID >= 80400)
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, 0);
#else
	class_entry = zend_register_internal_class_ex(&ce, NULL);
#endif

	return class_entry;
}
