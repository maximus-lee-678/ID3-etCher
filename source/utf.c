#include "../include/utf.h"

/* Private */
unsigned int _utf8_char_length(unsigned char val);
#define _UTF8_ALLOCATED_BYTES 5
#define _STARTING_MATRIX_LENGTH 16
#define _UTF8_LOCALE ".UTF-8"
#define _UTF8_CODE_PAGE 65001
#define _CMD_DEFAULT_CODE_PAGE 437

static unsigned int _console_default_code_page = 0;
utf8_matrix _failure_malformed_matrix = {UTF8_PARSE_MALFORMED, NULL, 0, 0};
utf8_matrix _failure_no_mem_matrix = {UTF8_PARSE_NO_MEM, NULL, 0, 0};

// Sets the locale of the program to UTF-8, overriding default code page usage for certain functions such as mkdir or fopen.
void utf8_set_locale() {
    setlocale(LC_ALL, _UTF8_LOCALE);
}

// Reverts locale to system defaults.
void utf8_unset_locale() {
    setlocale(LC_ALL, "");
}

// Gets the default code page of the console. If called after default code page is already initialised, does nothing.
void utf8_get_cp() {
    if (_console_default_code_page == 0) _console_default_code_page = GetConsoleCP();
}

// Sets the code page of the console to UTF-8, allowing for proper display of UTF-8 characters. Also obtains default code page if uninitialised.
void utf8_set_cp() {
    if (_console_default_code_page == 0) _console_default_code_page = GetConsoleCP();
    SetConsoleOutputCP(_UTF8_CODE_PAGE);
}

// Reverts to default code page. Call after init_cp or utf8_set_cp; if called before, active code page will be set to "US".
void utf8_unset_cp() {
    SetConsoleOutputCP(_console_default_code_page != 0 ? _console_default_code_page : _CMD_DEFAULT_CODE_PAGE);
}

/*
 * Parses a utf8 string into a utf8_matrix struct. See output of returned utf8_matrix struct for parse outcome.
 * Any return value of utf8_matrix_input->outcome which isn't UTF8_PARSE_SUCCESS indicates an error, see utf_8.h for error codes.
 * Failed matrixes do not need to be freed.
 * See definition of utf8_matrix struct for usage details.
 */
utf8_matrix *utf8_parse_string(char *string) {
    utf8_matrix *utf8_matrix_input = (utf8_matrix *)malloc(sizeof(utf8_matrix));
    if (utf8_matrix_input == NULL)  // check if malloc succeeds
        return &_failure_no_mem_matrix;

    utf8_matrix_input->outcome = UTF8_PARSE_WORKING;
    utf8_matrix_input->num_chars = 0;
    utf8_matrix_input->num_bytes = 0;

    utf8_matrix_input->string_matrix = (char **)malloc(_STARTING_MATRIX_LENGTH * sizeof(char *));
    if (utf8_matrix_input->string_matrix == NULL) {  // check if malloc succeeds
        utf8_free_matrix(utf8_matrix_input);
        return &_failure_no_mem_matrix;
    }

    for (int i = 0; i < _STARTING_MATRIX_LENGTH; i++) {
        utf8_matrix_input->string_matrix[i] = (char *)malloc(_UTF8_ALLOCATED_BYTES * sizeof(char));
        if (utf8_matrix_input->string_matrix[i] == NULL) {  // check if malloc succeeds
            utf8_matrix_input->num_chars = i;               // free up to i already allocated memory
            utf8_free_matrix(utf8_matrix_input);
            return &_failure_no_mem_matrix;
        }
    }

    unsigned int malloc_space = _STARTING_MATRIX_LENGTH;
    unsigned int examined_index = 0;
    unsigned int num_new_lines = 0;

    while (string[examined_index] != '\0') {
        // double memory if limit reached
        if (utf8_matrix_input->num_chars + 1 > malloc_space) {
            char **old_ptr = utf8_matrix_input->string_matrix;
            utf8_matrix_input->string_matrix = realloc(utf8_matrix_input->string_matrix, sizeof(char *) * (2 * malloc_space));
            if (utf8_matrix_input->string_matrix == NULL) {  // check if realloc succeeds
                utf8_matrix_input->string_matrix = old_ptr;
                utf8_free_matrix(utf8_matrix_input);
                return &_failure_no_mem_matrix;
            }

            for (int i = malloc_space; i < 2 * malloc_space; i++) {
                utf8_matrix_input->string_matrix[i] = (char *)malloc(_UTF8_ALLOCATED_BYTES * sizeof(char));
                if (utf8_matrix_input->string_matrix[i] == NULL) {  // check if malloc succeeds
                    utf8_matrix_input->num_chars = i;               // free up to i already allocated memory
                    utf8_free_matrix(utf8_matrix_input);
                    return &_failure_no_mem_matrix;
                }
            }

            malloc_space *= 2;
        }

        // carriage return is treated as one character, unintended as a standalone character, is part of newline sequence
        if (string[examined_index] == '\r') {
            examined_index++;
            num_new_lines++;
            continue;
        }

        unsigned int current_utf8_length = _utf8_char_length((unsigned char)string[examined_index]);

        if (current_utf8_length == 0) {
            utf8_free_matrix(utf8_matrix_input);
            return &_failure_malformed_matrix;
        }

        // at this point num_chars is 1 less than actual number of characters which corresponds to array index
        for (int i = 0; i < current_utf8_length; i++)
            utf8_matrix_input->string_matrix[utf8_matrix_input->num_chars][i] = string[examined_index++];
        utf8_matrix_input->string_matrix[utf8_matrix_input->num_chars][current_utf8_length] = '\0';

        utf8_matrix_input->num_chars++;
    }

    utf8_matrix_input->num_bytes = examined_index - num_new_lines;  // no need to minus one as examined_index is currently @ null terminator

    // free unused allocated memory
    for (int i = utf8_matrix_input->num_chars; i < malloc_space; i++)
        free(utf8_matrix_input->string_matrix[i]);

    utf8_matrix_input->outcome = UTF8_PARSE_SUCCESS;

    return utf8_matrix_input;
}

// Frees memory allocated to a utf8_matrix.
void utf8_free_matrix(utf8_matrix *matrix_instance) {
    // calling free on NULL is safe!
    for (int i = 0; i < matrix_instance->num_chars; i++)
        free(matrix_instance->string_matrix[i]);
    free(matrix_instance->string_matrix);
    free(matrix_instance);
}

/*
 * Function that accepts a character sequence and returns whether or not it contains a multibyte UTF-8 sequence.
 * Returns 1 if a multibyte sequence is found, 0 otherwise.
 * Returns -1 if the string is an invalid UTF8 string.
 */
int utf8_contains_multibyte_sequence(char *string) {
    unsigned int examined_index = 0;
    while (string[examined_index] != '\0') {
        unsigned int current_utf8_length = _utf8_char_length((unsigned char)string[examined_index]);
        switch (current_utf8_length) {
            case 0:
                return -1;
            case 1:
                examined_index++;
                break;
            default:
                return 1;
        }
    }

    return 0;
}

// Function to convert UTF-8 to UTF-16 with LE encoding
int utf8_to_utf16_le(const char *utf8_input_string, uint16_t **utf16_output_string, unsigned int *utf16_computed_length) {
    // in worst case, utf16 string is as wide as utf8 string, impossible to be wider
    *utf16_output_string = (uint16_t *)malloc((strlen(utf8_input_string) + 1) * sizeof(uint16_t));
    if (utf16_output_string == NULL)  // check if malloc succeeds
        return UTF16_PARSE_NO_MEM;

    uint16_t *output_ptr = *utf16_output_string;
    unsigned char *read_ptr = (unsigned char *)utf8_input_string;

    while (*read_ptr != '\0') {
        uint32_t codepoint = 0;
        unsigned int current_utf8_char_num_bytes = _utf8_char_length(*read_ptr);

        // determine the number of bytes in the UTF-8 character and extract the codepoint
        // yes i could write codepoint = *read_ptr++ but it's confusing
        switch (current_utf8_char_num_bytes) {
            case 0:
                free(*utf16_output_string);
                *utf16_output_string = NULL;
                return UTF16_PARSE_UTF8_MALFORMED;
            case 1:
                // in the case of 1 byte char, the whole char is the sequence
                // save contents of whole byte to codepoint, advance pointer by 1
                codepoint = *read_ptr;
                read_ptr++;
                break;
            case 2:
                // in the case of 2 byte char, the first 3 bytes of the first byte are encoding-related, mask them off
                // save masked contents of byte to codepoint, advance pointer by 1
                codepoint = *read_ptr & 0x1F;
                read_ptr++;
                break;
            case 3:
                // in the case of 3 byte char, the first 4 bytes of the first byte are encoding-related, mask them off
                // save masked contents of byte to codepoint, advance pointer by 1
                codepoint = *read_ptr & 0x0F;
                read_ptr++;
                break;
            case 4:
                // in the case of 4 byte char, the first 5 bytes of the first byte are encoding-related, mask them off
                // save masked contents of byte to codepoint, advance pointer by 1
                codepoint = *read_ptr & 0x07;
                read_ptr++;
                break;
        }

        // each time this loop iterates, the values currently in codepoint are shifted 6 bits to the right and
        // 6 bits at the current read pointer is copied into codepoint
        //
        // e.g. for a 2 byte utf8 char, e.g. before shifting: 0x0003, after shifting: 0x00C0
        //
        // the useful part of the current read pointer's value is only the first 6 bits, as each continuation byte is guaranteed to have "10" in bits 7 and 6
        // we mask off these bits then OR the output with codepoint to effectively add it in
        // e.g. before OR: 0x00C0, 
        // after: 0x00C0 | (0x83 & 0x3F)
        // = 0x00C0 | 0x03 
        // = 0x00C3
        
        // repeat until out of bytes
        for (unsigned int i = 1; i < current_utf8_char_num_bytes; i++) {
            // check if bits 7 and 6 are "10"
            if ((*read_ptr & 0xC0) != 0x80) {
                free(*utf16_output_string);
                *utf16_output_string = NULL;
                return UTF16_PARSE_UTF8_MALFORMED;
            }
            codepoint = (codepoint << 6) | (*read_ptr & 0x3F);
            read_ptr++;
        }

        // convert codepoint to utf16
        if (codepoint <= 0xFFFF) {
            // characters 1,2 or 3 bytes wide in utf8 encoded to utf16 fit into 16bits and can be written as is
            // write to output, then increment output pointer to prepare for next write
            *output_ptr = (uint16_t)codepoint;
            output_ptr++;
        } else {
            // characters 4 bytes in utf8 require 32bits, encoded as two surrogate pairs
            
            // 0x10000 is subtracted from the code point (U), leaving a 20-bit number (U') in the hex number range 0x00000–0xFFFFF
            codepoint -= 0x10000;
            // the high ten bits(in the range 0x000–0x3FF) are added to 0xD800 to give the first 16-bit code unit or high surrogate
            *output_ptr = (uint16_t)((codepoint >> 10) + 0xD800);
            output_ptr++;
            // the low ten bits (also in the range 0x000–0x3FF) are added to 0xDC00 to give the second 16-bit code unit or low surrogate
            *output_ptr = (uint16_t)((codepoint & 0x3FF) + 0xDC00);
            output_ptr++;
        }
    }

    *output_ptr = '\0';
    *utf16_computed_length = output_ptr - *utf16_output_string;
    return UTF16_PARSE_SUCCESS;
}

int utf8_to_utf16_be(const char *utf8_input_string, uint16_t **utf16_output_string, unsigned int *utf16_computed_length) {
    int utf16_fication_outcome = utf8_to_utf16_le(utf8_input_string, utf16_output_string, utf16_computed_length);
    if(utf16_fication_outcome != UTF16_PARSE_SUCCESS){
        return utf16_fication_outcome;
    }

    uint16_t *read_ptr = *utf16_output_string;
    // for LE encoding, the top and bottom 8 bits swap position
    for (unsigned int i = 0; i < *utf16_computed_length; ++i) {
        uint16_t value = read_ptr[i];
        read_ptr[i] = (value >> 8) | (value << 8);  // Swap high and low bytes
    }

    return UTF16_PARSE_SUCCESS;
}

// Internal function to determine how many bytes a UTF-8 character is based on its first byte.
unsigned int _utf8_char_length(unsigned char val) {
    // first byte of a UTF-8 character indicates how many bytes are in the character:
    // 0xxxxxxx - 1 byte character | 110xxxxx - 2 byte character | 1110xxxx - 3 byte character | 11110xxx - 4 byte character
    if ((val & (1 << 7)) == 0)
        return 1;
    else if ((val & (1 << 5)) == 0)
        return 2;
    else if ((val & (1 << 4)) == 0)
        return 3;
    else if ((val & (1 << 3)) == 0)
        return 4;
    else
        return 0;
}
