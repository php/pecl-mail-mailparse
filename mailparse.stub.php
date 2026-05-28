<?php

/**
 * @generate-class-entries
 * @generate-legacy-arginfo
 */

function mailparse_test(mixed $header): mixed {}

/**
 * @param resource $fp
 */
function mailparse_uudecode_all($fp): mixed {}

function mailparse_rfc822_parse_addresses(string $addresses): array {}

/**
 * @param resource $fp
 */
function mailparse_determine_best_xfer_encoding($fp): string {}

/**
 * @param resource $source_fp
 * @param resource $dest_fp
 */
function mailparse_stream_encode(
    $source_fp,
    $dest_fp,
    string $encoding
): bool {}

/**
 * @param resource $fp
 */
function mailparse_msg_parse($fp, string $data): bool {}

/**
 * @return resource|false
 */
function mailparse_msg_parse_file(string $filename){}

/**
 * @param resource $fp
 */
function mailparse_msg_free($fp): bool {}

/**
 * @return resource|false
 */
function mailparse_msg_create(){}

/**
 * @param resource $fp
 */
function mailparse_msg_get_structure($fp): array {}

/**
 * @param resource $fp
 */
function mailparse_msg_extract_part(
    $fp,
    string $msgbody,
    mixed $callback = null
): string|bool {}


/**
 * @param resource $fp
 */
function mailparse_msg_extract_whole_part_file(
    $fp,
    mixed $filename,
    mixed $callback = null
): string|bool {}


/**
 * @param resource $fp
 */
function mailparse_msg_extract_part_file(
    $fp,
    mixed $filename,
    mixed $callback = null
): string|bool {}


/**
 * @param resource $fp
 */
function mailparse_msg_get_part_data($fp): array {}


/**
 * @param resource $fp
 * @return resource|false
 */
function mailparse_msg_get_part($fp, string $data){}

class mimemessage
{
    public function __construct(mixed $mode, mixed $source) {}

    public function remove(): bool {}

    public function add_child(): bool {}

    public function get_child_count(): int {}

    public function get_parent(): ?mailparse_mimemessage {}

    public function get_child(mixed $item_to_find): ?mailparse_mimemessage {}

    public function extract_headers(
        mixed $mode = null,
        mixed $arg = null
    ): mixed {}

    public function extract_body(
        mixed $mode = null,
        mixed $arg = null
    ): mixed {}

    public function extract_uue(
        mixed $index,
        mixed $mode = null,
        mixed $arg = null
    ): mixed {}

    public function enum_uue(): array {}
}
