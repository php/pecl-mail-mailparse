# Version 3.2.0 - Unreleased

- Vendor encoding conversion code from mbstring. (alexdowad)
- Fix #44 Segmentation fault. (rlerdorf)
- Fix #20 Unexpected parsed value of content-id. (rlerdorf)

# Version 3.1.9 - 2025-09-30

- use Zend/zend_smart_string.h for PHP 8.5
- Fix memory leak

# Version 3.1.8 - 2024-10-04

- PHP 8.4 compatibility

# Version 3.1.7 - 2024-10-04

- PHP 8.4 compatibility

# Version 3.1.6 - 2023-08-22

- fix #29 Segmentation fault with ISO-2022-JP Subject header
- fix #30 Segmentation fault with UTF-8 encoded X-MS-Iris-MetaData header
- revert fix #81403 mailparse_rfc822_parse_addresses drops escaped quotes

# Version 3.1.5 - 2023-07-27

- drop usage of removed mbfl APIs in PHP 8.3
- fix GH-27 MimeMessage::__construct() throws TypeError with $mode=stream
- fix GH-21, GH-22, GH-24 segfault in mailparse_msg_parse without mime-version
- fix #81403 mailparse_rfc822_parse_addresses drops escaped quotes

# Version 3.1.4 - 2022-09-15

- declare mimemessage::data property
- drop support for PHP older than 7.3

# Version 3.1.3 - 2022-02-21

- Fix #73110: Mails with unknown MIME version are treated as plain/text. (cmb)
- Fix #74233: Parsing multi Content-Disposition causes memory leak. (cmb)
- Fix #75825: mailparse_uudecode_all doesn't parse multiple files. (cmb)
- Fix #81422: Potential double-free in mailparse_uudecode_all(). (cmb)
- Fix gh#19 Segmentation fault with PHP 8.1 in extract_body using MAILPARSE_EXTRACT_RETURN. (Remi)

# Version 3.1.2 - 2021-09-01

- Fix for PHP 8.1

# Version 3.1.1 - 2020-09-16

- Fixed bug #74215: Memory leaks with mailparse (cmb)
- Fixed bug #76498: Unable to use callable as callback (cmb)
- Compatibility with 8.0.0beta4

# Version 3.1.0 - 2020-04-22

- add arginfo to all functions
- fix MimeMessage constructor name

# Version 3.0.4 - 2019-12-19

- Replace ulong with zend_ulong, fix Windows build (cmb)

# Version 3.0.3 - 2019-03-20

QA Release:
- add missing files in archive
- fix -Wformat warning
- add dependency on mbstring extension
- PHP 7.3 compatibility

# Version 3.0.2 - 2016-12-07

- Fix segfault in getChild

# Version 3.0.1 - 2016-01-29

- Fix double free caused by mailparse_msg_free

# Version 3.0.0 - 2015-12-23

- PHP 7 Release
