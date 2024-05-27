#include "../include/id3_write.h"

#define _USE_28BIT_FORMAT_SIZE 0
#define _USE_32BIT_FORMAT_SIZE 1

uint8_t default_flags[2] = {0x00, 0x00};

// ["PRIVATE" FUNCTIONS] /////////////////////////////////////////////

void _write_text_tag(FILE* file_ptr, id3_text_tag_node* node);
void _write_comment_tag(FILE* file_ptr, id3_comment_tag_node* node);
void _write_picture_tag(FILE* file_ptr, id3_picture_tag_node* node);
void _integer_to_four_byte(unsigned int convertee, unsigned char* converted, int format_as);
//////////////////////////////////////////////////////////////////////

/*
 * <TEXT ENCODING> (encoding byte not included this description)
 * [UTF-16]
 * 0xFF 0xFE (Unicode BOM) + <string> + 0x00 0x00 (Unicode NULL)
 *
 * [ISO-8859-1 aka ASCII]
 * <string> + 0x00 (NULL terminator)
 */

/*
 * Writes tags to a file specified at file_path. Overwrites existing file if it exists.
 * - Pass in a id3_master_tag_struct which is initialised with the addresses of tag pointers.
 * - If any pointer to the id3_master_tag_struct is NULL, no tags of that type will be written.
 *
 * Usage:
 * id3_text_tag_node *tag_list = NULL;
 * id3_text_tag_node_add_update(&text_tag_list, "TALB", "Selection 3"); // repeat as many times as needed
 *
 * id3_master_tag_struct master_tag_collection;
 * id3_init_master_tag(&master_tag_collection);
 * master_tag_collection.text_tag_list = &tag_list;
 *
 * id3_write_tag("./song.mp3", master_tag_collection)
 */
void id3_write_tag(char* file_path, id3_master_tag_struct master_tag_collection) {
    /*
     * [ID3v2 main header overview]
     * File Identifier	"ID3" (0x49, 0x44, 0x33)
     * Version			$03 00
     * Flags			% abc00000 (tldr 0b00000000)
     * Size				4 * %0xxxxxxx (with 28bit technology)
     */
    uint8_t id3v2_header_without_size[6] = {0x49, 0x44, 0x33, 0x03, 0x00, 0x00};
    uint8_t id3v2_header_size_hex[4] = {0x00, 0x00, 0x00, 0x00};
    unsigned int id3v2_header_size = 0;

    // count size of text tags
    if (master_tag_collection.text_tag_list != NULL) {
        id3_text_tag_node* iter_node = *(master_tag_collection.text_tag_list);
        // size of each text tag is 10 (frame size) + string content
        while (iter_node != NULL) {
            id3v2_header_size += 10 + iter_node->num_id3_bytes;
            iter_node = iter_node->next;
        }
    }

    // count size of comment tags
    if (master_tag_collection.comment_tag_list != NULL) {
        id3_comment_tag_node* iter_node = *(master_tag_collection.comment_tag_list);
        // size of each comment tag is 10 (frame size) + string content
        while (iter_node != NULL) {
            id3v2_header_size += 10 + iter_node->num_id3_bytes;
            iter_node = iter_node->next;
        }
    }

    // count size of picture tags
    if (master_tag_collection.picture_tag_list != NULL) {
        id3_picture_tag_node* iter_node = *(master_tag_collection.picture_tag_list);
        // size of each picture tag is 10 (frame size) + content
        while (iter_node != NULL) {
            id3v2_header_size += 10 + iter_node->num_id3_bytes;
            iter_node = iter_node->next;
        }
    }

    // compute total size
    _integer_to_four_byte(id3v2_header_size, id3v2_header_size_hex, _USE_28BIT_FORMAT_SIZE);

    FILE* file_ptr;
    file_ptr = fopen(file_path, "wb");

    // write main header
    fwrite(id3v2_header_without_size, sizeof(id3v2_header_without_size), 1, file_ptr);
    fwrite(id3v2_header_size_hex, sizeof(id3v2_header_size_hex), 1, file_ptr);

    // write text tags
    if (master_tag_collection.text_tag_list != NULL) {
        id3_text_tag_node* iter_node = *(master_tag_collection.text_tag_list);
        while (iter_node != NULL) {
            _write_text_tag(file_ptr, iter_node);
            iter_node = iter_node->next;
        }
    }

    // write comment tags
    if (master_tag_collection.comment_tag_list != NULL) {
        id3_comment_tag_node* iter_node = *(master_tag_collection.comment_tag_list);
        while (iter_node != NULL) {
            _write_comment_tag(file_ptr, iter_node);
            iter_node = iter_node->next;
        }
    }

    // write picture tags
    if (master_tag_collection.picture_tag_list != NULL) {
        id3_picture_tag_node* iter_node = *(master_tag_collection.picture_tag_list);
        while (iter_node != NULL) {
            _write_picture_tag(file_ptr, iter_node);
            iter_node = iter_node->next;
        }
    }

    fclose(file_ptr);
}

/*
 * Sure, you could initialise a id3_master_tag_struct directly, but this ensures no segmentation faults occur by initialising everything to NULL.
 * - The "unsafe" way: id3_master_tag_struct master_tag_collection = {&tag_list, NULL, NULL}; // for text tags only
 *
 * Usage:
 * id3_master_tag_struct master_tag_collection;
 * id3_init_master_tag(&master_tag_collection);
 */
void id3_init_master_tag(id3_master_tag_struct* master_tag_collection) {
    master_tag_collection->comment_tag_list = NULL;
    master_tag_collection->picture_tag_list = NULL;
    master_tag_collection->text_tag_list = NULL;
}

/*
 * [INTERNAL FUNCTION]
 * Writes a single text tag to a file.
 * - Frame Header: https://id3.org/id3v2.3.0#ID3v2_frame_overview
 * - Frame Content: https://id3.org/id3v2.3.0#Text_information_frames
 */
void _write_text_tag(FILE* file_ptr, id3_text_tag_node* node) {
    /*
     * [Text frame overview]
     * Frame ID			$xx xx xx xx (four characters)
     * Size				$xx xx xx xx
     * Flags			$xx xx
     * Encoding			$xx (00: ISO-8859-1, 01: UTF-16)
     * Text				 <full text string according to encoding>
     */

    uint8_t id3v2_tag_size_hex[4] = {0x00, 0x00, 0x00, 0x00};
    _integer_to_four_byte(node->num_id3_bytes, id3v2_tag_size_hex, _USE_32BIT_FORMAT_SIZE);

    fprintf(file_ptr, node->tag_name);
    fwrite(id3v2_tag_size_hex, sizeof(id3v2_tag_size_hex), 1, file_ptr);
    fwrite(default_flags, sizeof(default_flags), 1, file_ptr);

    if (node->is_utf8) {
        fwrite("\x01\xFF\xFE", 3, 1, file_ptr);  // UTF-16 encoding indicator + BOM
        uint16_t* utf16_writer = node->tag_value_utf16;
        while (*utf16_writer != '\0') {
            fwrite(utf16_writer, 2, 1, file_ptr);
            utf16_writer++;
        }
        fwrite("\x00\x00", 2, 1, file_ptr);
    } else {
        fwrite("\x00", 1, 1, file_ptr);  // ISO-8859-1 encoding indicator
        fprintf(file_ptr, node->tag_value);
        fwrite("\x00", 1, 1, file_ptr);
    }
}

/*
 * [INTERNAL FUNCTION]
 * Writes a single comment tag to a file.
 * - Frame Header: https://id3.org/id3v2.3.0#ID3v2_frame_overview
 * - Frame Content: https://id3.org/id3v2.3.0#Comments
 */
void _write_comment_tag(FILE* file_ptr, id3_comment_tag_node* node) {
    /*
     * [Comment frame overview]
     * Frame ID			$xx xx xx xx (four characters)
     * Size				$xx xx xx xx
     * Flags			$xx xx
     * Encoding			$xx (00: ISO-8859-1, 01: UTF-16)
     * Language         $xx xx xx
     * Short content description  <text string according to encoding> $00 (00)
     * Text             <full text string according to encoding>
     */

    unsigned char id3v2_tag_size_hex[4] = {0x00, 0x00, 0x00, 0x00};
    _integer_to_four_byte(node->num_id3_bytes, id3v2_tag_size_hex, _USE_32BIT_FORMAT_SIZE);

    fprintf(file_ptr, _TAG_NAME_COMMENT);
    fwrite(id3v2_tag_size_hex, sizeof(id3v2_tag_size_hex), 1, file_ptr);
    fwrite(default_flags, sizeof(default_flags), 1, file_ptr);

    if (node->is_utf8) {
        fwrite("\x01", 1, 1, file_ptr);          // UTF-16 encoding indicator
        fwrite(node->language, 3, 1, file_ptr);  // language

        // short content description
        fwrite("\xFF\xFE", 2, 1, file_ptr);  // BOM
        uint16_t* utf16_writer = node->short_content_description_utf16;
        while (*utf16_writer != '\0') {
            fwrite(utf16_writer, 2, 1, file_ptr);
            utf16_writer++;
        }
        fwrite("\x00\x00", 2, 1, file_ptr);

        // comment
        fwrite("\xFF\xFE", 2, 1, file_ptr);  // BOM
        utf16_writer = node->comment_utf16;
        while (*utf16_writer != '\0') {
            fwrite(utf16_writer, 2, 1, file_ptr);
            utf16_writer++;
        }
        fwrite("\x00\x00", 2, 1, file_ptr);
    } else {
        fwrite("\x00", 1, 1, file_ptr);          // ISO-8859-1 encoding indicator
        fwrite(node->language, 3, 1, file_ptr);  // language

        fprintf(file_ptr, node->short_content_description);  // short content description
        fwrite("\x00", 1, 1, file_ptr);

        fprintf(file_ptr, node->comment);  // comment
        fwrite("\x00", 1, 1, file_ptr);
    }
}

/*
 * [INTERNAL FUNCTION]
 * Writes a single picture tag to a file.
 * - Frame Header: https://id3.org/id3v2.3.0#ID3v2_frame_overview
 * - Frame Content: https://id3.org/id3v2.3.0#Attached_picture
 */
void _write_picture_tag(FILE* file_ptr, id3_picture_tag_node* node) {
    /*
     * [Picture Frame overview]
     * Text encoding   $xx
     * MIME type       <text string> $00
     * Picture type    $xx
     * Description     <text string according to encoding> $00 (00)
     * Picture data    <binary data>
     */

    unsigned char id3v2_tag_size_hex[4] = {0x00, 0x00, 0x00, 0x00};
    _integer_to_four_byte(node->num_id3_bytes, id3v2_tag_size_hex, _USE_32BIT_FORMAT_SIZE);

    fprintf(file_ptr, _TAG_NAME_PICTURE);
    fwrite(id3v2_tag_size_hex, sizeof(id3v2_tag_size_hex), 1, file_ptr);
    fwrite(default_flags, sizeof(default_flags), 1, file_ptr);

    if (node->is_utf8) {
        fwrite("\x01", 1, 1, file_ptr);      // UTF-16 encoding indicator
        fprintf(file_ptr, node->mime_type);  // mime type
        fwrite("\x00", 1, 1, file_ptr);
        fwrite(&node->picture_type, 1, 1, file_ptr);  // picture type (fwrite needs pointer so we &)

        // description
        fwrite("\xFF\xFE", 2, 1, file_ptr);  // BOM
        uint16_t* utf16_writer = node->description_utf16;
        while (*utf16_writer != '\0') {
            fwrite(utf16_writer, 2, 1, file_ptr);
            utf16_writer++;
        }
        fwrite("\x00\x00", 2, 1, file_ptr);
    } else {
        fwrite("\x00", 1, 1, file_ptr);      // ISO-8859-1 encoding indicator
        fprintf(file_ptr, node->mime_type);  // mime type
        fwrite("\x00", 1, 1, file_ptr);
        fwrite(&node->picture_type, 1, 1, file_ptr);  // picture type (fwrite needs pointer so we &)

        fprintf(file_ptr, node->description);  //  description
        fwrite("\x00", 1, 1, file_ptr);
    }

    if (node->is_picture_stored_as_file) {
        FILE* picture_file_ptr;
        picture_file_ptr = fopen(node->picture_file_path, "rb");

        // picture guaranteed to have been opened before during processing
        int a;
        while ((a = fgetc(picture_file_ptr)) != EOF)
            fputc(a, file_ptr);

        fclose(picture_file_ptr);
    } else {
        fwrite(node->picture_binary_data, node->picture_binary_data_bytes, 1, file_ptr);
    }
}

void _integer_to_four_byte(unsigned int convertee, uint8_t* converted, int format_as) {
    if (format_as == _USE_28BIT_FORMAT_SIZE) {
        // The ID3v2 tag size is encoded with four bytes where the most significant bit (bit 7) is set to zero in every byte,
        // making a total of 28 bits. The zeroed bits are ignored, so a 257 bytes long tag is represented as $00 00 02 01.

        // The ID3v2 tag size is the size of the complete tag after unsychronisation, including padding,
        // excluding the header but not excluding the extended header(total tag size - 10). Only 28 bits(representing up to 256MB)
        // are used in the size description to avoid the introducuction of 'false syncsignals'.

        // Because of this, we shift bits 3 times.

        unsigned int temp = 0;

        temp = convertee & 0xFFFFFF80;       // Pull bits 31-7 (last 24)
        temp <<= 1;                          // Left shift
        convertee = convertee & 0x0000007F;  // Zeroise all but first 7 bits
        convertee |= temp;                   // Merge back

        temp = convertee & 0xFFFF8000;       // Pull bits 31-15 (last 16)
        temp <<= 1;                          // Left shift
        convertee = convertee & 0x00007FFF;  // Zeroise all but first 15 bits
        convertee |= temp;                   // Merge back

        temp = convertee & 0xFF800000;       // Pull bits 31-23 (last 8)
        temp <<= 1;                          // Left shift
        convertee = convertee & 0x007FFFFF;  // Zeroise all but first 23 bits
        convertee |= temp;                   // Merge back
    }

    converted[0] = (convertee >> 24) & 0xFF;
    converted[1] = (convertee >> 16) & 0xFF;
    converted[2] = (convertee >> 8) & 0xFF;
    converted[3] = convertee & 0xFF;
}
