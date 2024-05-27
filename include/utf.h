#pragma once

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>

// <3 mojibake 4evr

#define UTF8_PARSE_SUCCESS 0
#define UTF8_PARSE_MALFORMED 1
#define UTF8_PARSE_NO_MEM 2
#define UTF8_PARSE_WORKING 3

#define UTF16_PARSE_SUCCESS 4
#define UTF16_PARSE_UTF8_MALFORMED 5
#define UTF16_PARSE_NO_MEM 6

/*
 * Struct which holds a utf8 string in a matrix. Useful for when you need to iterate over each character in a utf8 string.
 *
 * outcome: Outcome of the parse operation. See utf_8.h for possible outcomes.
 * string_matrix: Matrix of UTF8 characters. Iterate over this num_chars times to retrieve each utf8 character. New lines occupy 1 byte (\n, not \r\n).
 * WARNING: Do not attempt to access string_matrix if outcome is not UTF8_PARSE_SUCCESS, as it will result in undefined behavior.
 * num_chars: Total number of characters in string_matrix.
 * num_bytes: Sum of bytes in all string_matrix values, not including null terminators in each matrix value.
 */
typedef struct {
    unsigned int outcome;
    char** string_matrix;
    unsigned int num_chars;
    unsigned int num_bytes;
} utf8_matrix;

// [UTF-8]

void utf8_set_locale();
void utf8_unset_locale();
void utf8_get_cp();
void utf8_set_cp();
void utf8_unset_cp();
utf8_matrix* utf8_parse_string(char* string);
void utf8_free_matrix(utf8_matrix* metadata);

// [UTF-16]

int utf8_contains_multibyte_sequence(char *string);
int utf8_to_utf16_le(const char* utf8_input_string, uint16_t** utf16_output_string, unsigned int* utf16_computed_length);
int utf8_to_utf16_be(const char* utf8_input_string, uint16_t** utf16_output_string, unsigned int* utf16_computed_length);
