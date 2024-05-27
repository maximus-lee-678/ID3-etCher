#include "../include/id3_process.h"

#define _NUM_TEXT_TAGS 38
#define _TAG_NAME_LENGTH 5
#define _COMMENT_LANGUAGE_LENGTH 3

#define _ENCODING_BYTE_LENGTH 1
#define _ENCODING_UNICODE_BOM_LENGTH 2
#define _ENCODING_UNICODE_NULL_LENGTH 2
#define _ENCODING_ISO_NULL_LENGTH 1
#define _ENCODING_APIC_PICTURE_TYPE_LENGTH 1

#define _NODE_TYPE_TEXT 0
#define _NODE_TYPE_COMMENT 1
#define _NODE_TYPE_APIC 2

// ["PRIVATE" FUNCTIONS] /////////////////////////////////////////////

unsigned int _node_generate_metadata(int node_type, void** node);
unsigned int _text_node_generate_metadata(id3_text_tag_node** node);
unsigned int _comment_node_generate_metadata(id3_comment_tag_node** node);
unsigned int _picture_node_generate_metadata(id3_picture_tag_node** node);
void _free_text_tag_node(id3_text_tag_node* node);
void _free_comment_tag_node(id3_comment_tag_node* node);
void _free_picture_tag_node(id3_picture_tag_node* node);
//////////////////////////////////////////////////////////////////////

/*
 * To be consistent throughout this source file, we will free EVERYTHING that was attempted to be malloced, even if it's guaranteed to be NULL.
 * This may result in redundant looking frees, but free(NULL) does nothing anyways.
 * e.g.
 * iter_node->tag_value = (char*)malloc(strlen(tag_value) + 1);
 * if (iter_node->tag_value == NULL)
 *     free(iter_node->tag_value);
 */

/*
 * The text information frames are the most important frames, containing information like artist, album and more.
 * There may only be one text information frame of its kind in an tag, with the exception of "TXXX", which may be present more than once.
 * All text frame identifiers begin with "T".
 */
char TEXT_TAGS[_NUM_TEXT_TAGS][_TAG_NAME_LENGTH] = {
    "TALB", "TBPM", "TCOM", "TCON", "TCOP", "TDAT", "TDLY", "TENC", "TEXT",
    "TFLT", "TIME", "TIT1", "TIT2", "TIT3", "TKEY", "TLAN", "TLEN", "TMED",
    "TOAL", "TOFN", "TOLY", "TOPE", "TORY", "TOWN", "TPE1", "TPE2", "TPE3",
    "TPE4", "TPOS", "TPUB", "TRCK", "TRDA", "TRSN", "TRSO", "TSIZ", "TSRC", "TSSE", "TYER"};

/*
 * Adds a new node to the end of a text tag linked list if the tag_name doesn't already exist in it.
 * If a matching tag is found, it replaces that node's tag_value with the provided tag_value and refreshes the utf8 and utf16 metadata fields.
 * - This function requires tag_value to be a non-zero length string.
 * - If the operation fails, the provided linked list remains unchanged.
 * - See id3_write.c's _write_text_tag() for more information on the format of the tag.
 *
 * Usage:
 * id3_text_tag_node* text_tag_list = NULL;
 * id3_text_tag_node_add_update(&text_tag_list, "TALB", "Selection 3"); // repeat as many times as needed
 *
 * Returns (success): NODE_ADD_SUCCESS, NODE_UPDATE_SUCCESS
 * Returns (failure): NODE_FILE_ERROR, NODE_INVALID_TAG_VALUE, NODE_MEMORY_ERROR, UTF8_PARSE_MALFORMED, UTF16_PARSE_NO_MEM, UTF16_PARSE_UTF8_MALFORMED
 */
unsigned int id3_text_tag_node_add_update(id3_text_tag_node** head, char* tag_name, char* tag_value) {
    unsigned int is_valid_text_tag = 0;
    for (int i = 0; i < _NUM_TEXT_TAGS; i++) {
        if (!strcmp(tag_name, TEXT_TAGS[i])) {
            is_valid_text_tag = 1;
            break;
        }
    }
    if (!is_valid_text_tag)
        return NODE_INVALID_TAG_NAME;

    if (strlen(tag_value) == 0)
        return NODE_INVALID_TAG_VALUE;

    // below, we iterate to find node that matches update criteria, update values as appropriate if found
    // if no matching node found, add a new node after the last node
    id3_text_tag_node* last_node = NULL;

    if (*head != NULL) {
        id3_text_tag_node* iter_node = *head;

        while (1) {
            if (!strcmp(iter_node->tag_name, tag_name)) {
                // checkpointing previous values
                char* old_tag_value = iter_node->tag_value;

                iter_node->tag_value = (char*)malloc(strlen(tag_value) + 1);

                // malloc check
                if (iter_node->tag_value == NULL) {
                    free(iter_node->tag_value);

                    iter_node->tag_value = old_tag_value;

                    return NODE_MEMORY_ERROR;
                }

                strncpy(iter_node->tag_value, tag_value, strlen(tag_value) + 1);

                // in event of parse failure, revert to old values
                unsigned int metadata_parse_outcome = _node_generate_metadata(_NODE_TYPE_TEXT, (void**)&iter_node);
                if (metadata_parse_outcome != UTF8_PARSE_SUCCESS) {
                    free(iter_node->tag_value);

                    iter_node->tag_value = old_tag_value;

                    return metadata_parse_outcome;
                }

                // cleanup old values once new assignments complete
                free(old_tag_value);

                return NODE_UPDATE_SUCCESS;
            }

            if (iter_node->next != NULL) {
                iter_node = iter_node->next;
            } else {
                last_node = iter_node;
                break;
            }
        }
    }

    // create a new node
    id3_text_tag_node* new_node = (id3_text_tag_node*)malloc(sizeof(id3_text_tag_node));

    // malloc check -> struct
    if (new_node == NULL)
        return NODE_MEMORY_ERROR;

    *new_node = (id3_text_tag_node){.next = NULL, .num_id3_bytes = 0, .is_utf8 = 0, .tag_value_utf16 = NULL};

    new_node->tag_value = (char*)malloc(strlen(tag_value) + 1);

    // malloc check -> struct members
    if (new_node->tag_value == NULL) {
        free(new_node->tag_value);
        free(new_node);

        return NODE_MEMORY_ERROR;
    }

    strncpy(new_node->tag_name, tag_name, _TAG_NAME_LENGTH);
    strncpy(new_node->tag_value, tag_value, strlen(tag_value) + 1);

    // in event of parse failure, revert to old values
    unsigned int metadata_parse_outcome = _node_generate_metadata(_NODE_TYPE_TEXT, (void**)&new_node);
    if (metadata_parse_outcome != UTF8_PARSE_SUCCESS) {
        free(new_node->tag_value);
        free(new_node);

        return metadata_parse_outcome;
    }

    if (*head == NULL)
        *head = new_node;
    else
        last_node->next = new_node;

    return NODE_ADD_SUCCESS;
}

/*
 * Deletes a specified node of a text tag linked list.
 * - If you want to delete a "TXXX" tag, use id3_txxx_tag_node_delete() instead.
 * - If a node with a matching tag_name is not found, the linked list remains unchanged.
 *
 * Usage:
 * id3_text_tag_node* text_tag_list = NULL;
 * id3_text_tag_node_add_update(&text_tag_list, "TALB", "Selection 3");
 * id3_text_tag_node_delete(&text_tag_list, "TALB");
 *
 * Returns (success): NODE_DELETE_SUCCESS
 * Returns (failure): NODE_NOT_FOUND, NODE_INVALID_HEAD
 */
unsigned int id3_text_tag_node_delete(id3_text_tag_node** head, char* tag_name) {
    id3_text_tag_node* iter_node = *head;
    id3_text_tag_node* iter_node_prev = NULL;

    // Empty linked list given
    if (iter_node == NULL)
        return NODE_INVALID_HEAD;

    do {
        if (!strcmp(iter_node->tag_name, tag_name)) {
            // node is not head, either joins the previous node to the next node or sets the previous node's next to NULL
            if (iter_node_prev != NULL)
                iter_node_prev->next = iter_node->next;
            // node is head, make the next node the new head
            else
                *head = iter_node->next;

            _free_text_tag_node(iter_node);

            return NODE_DELETE_SUCCESS;
        }

        iter_node_prev = iter_node;
        iter_node = iter_node->next;
    } while (iter_node != NULL);

    return NODE_NOT_FOUND;
}

/*
 * Frees memory of an entire text tag linked list based on a pointer to a head pointer.
 * - The original pointer will then be set to NULL.
 *
 * Usage:
 * id3_text_tag_node* text_tag_list = NULL;
 * id3_text_tag_node_add_update(&text_tag_list, "TALB", "Selection 3");
 * id3_text_tag_list_destroy(&text_tag_list);
 */
void id3_text_tag_list_destroy(id3_text_tag_node** head) {
    id3_text_tag_node* iter_node = *head;
    id3_text_tag_node* free_node = NULL;

    while (iter_node != NULL) {
        free_node = iter_node;
        iter_node = iter_node->next;

        _free_text_tag_node(free_node);
    }

    *head = NULL;
}

/*
 * Adds a new node to the end of a comment tag linked list if a node with a matching language and short_content_description doesn't already exist in it.
 * If a matching tag is found, it replaces that node's comment value with the provided comment value and refreshes the utf8 and utf16 metadata fields.
 * - This function requires the language value to be exactly 3 characters and the comment value to be a non-zero length string.
 * - short_content_description can be an empty string.
 * - [Mp3tag] If the resulting header is to be read by Mp3tag, short_content_description must be an empty string or the tag will be deemed corrupt.
 * - If the operation fails, the provided linked list remains unchanged.
 * - See id3_write.c's _write_comment_tag() for more information on the format of the tag.
 *
 * Usage:
 * id3_comment_tag_node* comment_tag_list = NULL;
 * id3_comment_tag_node_add_update(&comment_tag_list, "eng", "", "Tag, you're it!"); // repeat as many times as needed
 *
 * Returns (success): NODE_ADD_SUCCESS, NODE_UPDATE_SUCCESS
 * Returns (failure): NODE_FILE_ERROR, NODE_INVALID_TAG_VALUE, NODE_MEMORY_ERROR, UTF8_PARSE_MALFORMED, UTF16_PARSE_NO_MEM, UTF16_PARSE_UTF8_MALFORMED
 */
unsigned int id3_comment_tag_node_add_update(id3_comment_tag_node** head, char* language, char* short_content_description, char* comment) {
    if (strlen(language) != _COMMENT_LANGUAGE_LENGTH || strlen(comment) == 0)
        return NODE_INVALID_TAG_VALUE;

    // below, we iterate to find node that matches update criteria, update values as appropriate if found
    // if no matching node found, add a new node after the last node
    id3_comment_tag_node* last_node = NULL;

    if (*head != NULL) {
        id3_comment_tag_node* iter_node = *head;

        while (1) {
            if (!strcmp(iter_node->language, language) && !strcmp(iter_node->short_content_description, short_content_description)) {
                // checkpointing previous values
                char* old_comment = iter_node->comment;

                iter_node->comment = (char*)malloc(strlen(comment) + 1);

                // malloc check
                if (iter_node->comment == NULL) {
                    free(iter_node->comment);

                    iter_node->comment = old_comment;

                    return NODE_MEMORY_ERROR;
                }

                strncpy(iter_node->comment, comment, strlen(comment) + 1);

                // in event of parse failure, revert to old values
                unsigned int metadata_parse_outcome = _node_generate_metadata(_NODE_TYPE_COMMENT, (void**)&iter_node);
                if (metadata_parse_outcome != UTF8_PARSE_SUCCESS) {
                    free(iter_node->comment);

                    iter_node->comment = old_comment;

                    return NODE_INVALID_TAG_VALUE;
                }

                // cleanup old values once new assignments complete
                free(old_comment);

                return NODE_UPDATE_SUCCESS;
            }

            if (iter_node->next != NULL) {
                iter_node = iter_node->next;
            } else {
                last_node = iter_node;
                break;
            }
        }
    }

    // create a new node
    id3_comment_tag_node* new_node = (id3_comment_tag_node*)malloc(sizeof(id3_comment_tag_node));

    // malloc check -> struct
    if (new_node == NULL)
        return NODE_MEMORY_ERROR;

    *new_node = (id3_comment_tag_node){.next = NULL, .num_id3_bytes = 0, .is_utf8 = 0, .short_content_description_utf16 = NULL, .comment_utf16 = NULL};

    new_node->short_content_description = (char*)malloc(strlen(short_content_description) + 1);
    new_node->comment = (char*)malloc(strlen(comment) + 1);

    // malloc check -> struct members
    if (new_node->short_content_description == NULL || new_node->comment == NULL) {
        free(new_node->short_content_description);
        free(new_node->comment);
        free(new_node);

        return NODE_MEMORY_ERROR;
    }

    strncpy(new_node->language, language, _COMMENT_LANGUAGE_LENGTH + 1);
    strncpy(new_node->short_content_description, short_content_description, strlen(short_content_description) + 1);
    strncpy(new_node->comment, comment, strlen(comment) + 1);

    // in event of parse failure, revert to old values
    unsigned int metadata_parse_outcome = _node_generate_metadata(_NODE_TYPE_COMMENT, (void**)&new_node);
    if (metadata_parse_outcome != UTF8_PARSE_SUCCESS) {
        free(new_node->short_content_description);
        free(new_node->comment);
        free(new_node);

        return metadata_parse_outcome;
    }

    if (*head == NULL)
        *head = new_node;
    else
        last_node->next = new_node;

    return NODE_ADD_SUCCESS;
}

/*
 * Deletes a specified node of a comment tag linked list.
 * - If a node with a matching language and short_content_description is not found, the linked list remains unchanged.
 *
 * Usage:
 * comment_tag_node* comment_tag_list = NULL;
 * id3_comment_tag_node_add_update(&comment_tag_list, "eng", "", "Tag, you're it!");
 * id3_comment_tag_node_delete(&comment_tag_list, "eng", "");
 *
 * Returns (success): NODE_DELETE_SUCCESS
 * Returns (failure): NODE_NOT_FOUND, NODE_INVALID_HEAD
 */
unsigned int id3_comment_tag_node_delete(id3_comment_tag_node** head, char* language, char* short_content_description) {
    id3_comment_tag_node* iter_node = *head;
    id3_comment_tag_node* iter_node_prev = NULL;

    // Empty linked list given
    if (iter_node == NULL)
        return NODE_INVALID_HEAD;

    do {
        if (!strcmp(iter_node->language, language) && !strcmp(iter_node->short_content_description, short_content_description)) {
            // node is not head, either joins the previous node to the next node or sets the previous node's next to NULL
            if (iter_node_prev != NULL)
                iter_node_prev->next = iter_node->next;
            // node is head, make the next node the new head
            else
                *head = iter_node->next;

            _free_comment_tag_node(iter_node);

            return NODE_DELETE_SUCCESS;
        }

        iter_node_prev = iter_node;
        iter_node = iter_node->next;
    } while (iter_node != NULL);

    return NODE_NOT_FOUND;
}

/*
 * Frees memory of an entire comment tag linked list based on a pointer to a head pointer.
 * - The original pointer will then be set to NULL.
 *
 * Usage:
 * id3_comment_tag_node* comment_tag_list = NULL;
 * id3_comment_tag_node_add_update(&comment_tag_list, "eng", "", "Tag, you're it!");
 * id3_comment_tag_list_destroy(&comment_tag_list);
 */
void id3_comment_tag_list_destroy(id3_comment_tag_node** head) {
    id3_comment_tag_node* iter_node = *head;
    id3_comment_tag_node* free_node = NULL;

    while (iter_node != NULL) {
        free_node = iter_node;
        iter_node = iter_node->next;

        _free_comment_tag_node(free_node);
    }

    *head = NULL;
}

/*
 * Modifies an existing node or adds a new node to the end of a picture tag linked list.
 * - If picture type is specified to be APIC_TYPE_FILE_ICON (0x01) or APIC_TYPE_OTHER_FILE_ICON (0x02), the function will attempt
 *   to locate a node with a matching picture type and replace its mime_type, description and (picture_file_path or binary_data) with the provided values.
 * - For any other picture type, the function will attempt to locate a node with a matching description and replace its
 *   mime_type, picture_type, and (picture_file_path or binary_data) with the provided values.
 * - This implements the specification that there may only be one picture with the picture type declared as picture type $01 and $02 respectively.
 * - Provide EITHER picture_file_path OR (picture_binary_data AND picture_binary_data_bytes), not both.
 * - mime_type and picture_type are mandatory. picture_type must be between 0x00 and 0x14 inclusive (can use APIC_TYPE constants).
 * - description can be an empty string.
 * - If the operation fails, the provided linked list remains unchanged.
 * - See id3_write.c's _write_picture_tag() for more information on the format of the tag.
 *
 * Usage:
 * id3_picture_tag_node *picture_tag_list = NULL;
 * id3_picture_tag_node_add_update(&picture_tag_list, "image/jpeg", APIC_TYPE_COVER_FRONT, "FRONT", "./folder.jpg", NULL, 0);
 * id3_picture_tag_node_add_update(&picture_tag_list, "image/jpeg", APIC_TYPE_COVER_BACK, "", NULL, buffer, buffer_size); // repeat as many times as needed
 *
 * Returns (success): NODE_ADD_SUCCESS, NODE_UPDATE_SUCCESS
 * Returns (failure): NODE_FILE_ERROR, NODE_INVALID_TAG_VALUE, NODE_MEMORY_ERROR, UTF8_PARSE_MALFORMED, UTF16_PARSE_NO_MEM, UTF16_PARSE_UTF8_MALFORMED, NODE_FILE_ERROR
 */
unsigned int id3_picture_tag_node_add_update(id3_picture_tag_node** head, char* mime_type, uint8_t picture_type, char* description,
                                             char* picture_file_path, uint8_t* picture_binary_data, unsigned int picture_binary_data_bytes) {
    enum enum_update_operation {
        UPDATE_NONE,
        UPDATE_TYPE_FILE_ICON,
        UPDATE_TYPE_STANDARD
    };

    if (strlen(mime_type) == 0 || picture_type > APIC_TYPE_PUBLISHER_STUDIO_LOGOTYPE)
        return NODE_INVALID_TAG_VALUE;

    if (picture_file_path != NULL && picture_binary_data != NULL)
        return NODE_INVALID_TAG_VALUE;

    // below, we iterate to find node that matches update criteria, update values as appropriate if found
    // if no matching node found, add a new node after the last node
    id3_picture_tag_node* last_node = NULL;

    if (*head != NULL) {
        enum enum_update_operation update_operation = UPDATE_NONE;
        id3_picture_tag_node* iter_node = *head;

        while (1) {
            if ((picture_type == APIC_TYPE_FILE_ICON || picture_type == APIC_TYPE_OTHER_FILE_ICON) && iter_node->picture_type == picture_type) {
                // there may only be one picture with the picture type declared as picture type $01 and $02 respectively.
                // we update mime_type, description, and picture path or picture binary data.
                update_operation = UPDATE_TYPE_FILE_ICON;
            } else if (iter_node->picture_type == picture_type && !strcmp(iter_node->description, description)) {
                // there may be several pictures attached to one file, each in their individual "APIC" frame, but only one with the same content descriptor.
                //  we update mime_type, and picture path or picture binary data.
                update_operation = UPDATE_TYPE_STANDARD;
            }

            if (update_operation != UPDATE_NONE) {
                // checkpointing previous values
                char* old_mime_type = iter_node->mime_type;
                char* old_description = update_operation == UPDATE_TYPE_FILE_ICON ? iter_node->description : NULL;
                char* old_picture_file_path = iter_node->picture_file_path;

                iter_node->mime_type = (char*)malloc(strlen(mime_type) + 1);
                if (update_operation == UPDATE_TYPE_FILE_ICON) iter_node->description = (char*)malloc(strlen(description) + 1);
                if (picture_file_path != NULL) iter_node->picture_file_path = (char*)malloc(strlen(picture_file_path) + 1);

                // malloc check
                if (iter_node->mime_type == NULL || iter_node->description == NULL || (picture_file_path != NULL && iter_node->picture_file_path == NULL)) {
                    free(iter_node->mime_type);
                    free(iter_node->description);
                    free(iter_node->picture_file_path);

                    iter_node->mime_type = old_mime_type;
                    if (update_operation == UPDATE_TYPE_FILE_ICON) iter_node->description = old_description;
                    iter_node->picture_file_path = old_picture_file_path;

                    return NODE_MEMORY_ERROR;
                }

                strncpy(iter_node->mime_type, mime_type, strlen(mime_type) + 1);
                if (update_operation == UPDATE_TYPE_FILE_ICON) strncpy(iter_node->description, description, strlen(description) + 1);
                if (picture_file_path != NULL) {
                    strncpy(iter_node->picture_file_path, picture_file_path, strlen(picture_file_path) + 1);
                    iter_node->is_picture_stored_as_file = 1;
                }
                if (picture_binary_data != NULL) {
                    iter_node->picture_binary_data = picture_binary_data;
                    iter_node->is_picture_stored_as_file = 0;
                }

                // in event of parse failure, revert to old values
                unsigned int metadata_parse_outcome = _node_generate_metadata(_NODE_TYPE_APIC, (void**)&iter_node);
                if (metadata_parse_outcome != UTF8_PARSE_SUCCESS) {
                    free(iter_node->mime_type);
                    free(iter_node->description);
                    free(iter_node->picture_file_path);

                    iter_node->mime_type = old_mime_type;
                    if (update_operation == UPDATE_TYPE_FILE_ICON) iter_node->description = old_description;
                    iter_node->picture_file_path = old_picture_file_path;

                    return metadata_parse_outcome;
                }

                // cleanup old values once new assignments complete
                free(old_mime_type);
                free(old_description);
                free(old_picture_file_path);

                return NODE_UPDATE_SUCCESS;
            }

            if (iter_node->next != NULL) {
                iter_node = iter_node->next;
            } else {
                last_node = iter_node;
                break;
            }
        }
    }

    // create a new node
    id3_picture_tag_node* new_node = (id3_picture_tag_node*)malloc(sizeof(id3_picture_tag_node));

    // malloc check -> struct
    if (new_node == NULL)
        return NODE_MEMORY_ERROR;

    *new_node = (id3_picture_tag_node){.next = NULL, .mime_type = NULL, .picture_type = 0x00, .description = NULL, .num_id3_bytes = 0, .is_utf8 = 0, .description_utf16 = NULL, .is_picture_stored_as_file = 0, .picture_file_path = NULL, .picture_binary_data = NULL, .picture_binary_data_bytes = 0};

    new_node->mime_type = (char*)malloc(strlen(mime_type) + 1);
    new_node->description = (char*)malloc(strlen(description) + 1);
    if (picture_file_path != NULL) new_node->picture_file_path = (char*)malloc(strlen(picture_file_path) + 1);

    // malloc check -> struct members
    if (new_node->mime_type == NULL || new_node->description == NULL || (picture_file_path != NULL && new_node->picture_file_path == NULL)) {
        free(new_node->mime_type);
        free(new_node->description);
        free(new_node->picture_file_path);
        free(new_node);

        return NODE_MEMORY_ERROR;
    }

    new_node->picture_type = picture_type;
    strncpy(new_node->mime_type, mime_type, strlen(mime_type) + 1);
    strncpy(new_node->description, description, strlen(description) + 1);
    if (picture_file_path != NULL) {
        strncpy(new_node->picture_file_path, picture_file_path, strlen(picture_file_path) + 1);
        new_node->is_picture_stored_as_file = 1;
    } else {
        new_node->picture_binary_data = picture_binary_data;
        new_node->picture_binary_data_bytes = picture_binary_data_bytes;
        new_node->is_picture_stored_as_file = 0;
    }

    // in event of parse failure, revert to old values
    unsigned int metadata_parse_outcome = _node_generate_metadata(_NODE_TYPE_APIC, (void**)&new_node);
    if (metadata_parse_outcome != UTF8_PARSE_SUCCESS) {
        free(new_node->mime_type);
        free(new_node->description);
        free(new_node->picture_file_path);
        free(new_node);

        return NODE_INVALID_TAG_VALUE;
    }

    if (*head == NULL)
        *head = new_node;
    else
        last_node->next = new_node;

    return NODE_ADD_SUCCESS;
}

/*
 * Deletes a specified node of a picture tag linked list.
 * - If a node with a matching picture_type and description is not found, the linked list remains unchanged.
 *
 * Usage:
 * id3_picture_tag_node* picture_tag_list = NULL;
 * id3_picture_tag_node_add_update(&picture_tag_list, "image/jpeg", APIC_TYPE_COVER_FRONT, "FRONT", "./folder.jpg", NULL, 0);
 * id3_picture_tag_node_delete(&picture_tag_list, APIC_TYPE_COVER_FRONT, "FRONT");
 *
 * Returns (success): NODE_DELETE_SUCCESS
 * Returns (failure): NODE_NOT_FOUND, NODE_INVALID_HEAD
 */
unsigned int id3_picture_tag_node_delete(id3_picture_tag_node** head, uint8_t picture_type, char* description) {
    id3_picture_tag_node* iter_node = *head;
    id3_picture_tag_node* iter_node_prev = NULL;

    // Empty linked list given
    if (iter_node == NULL)
        return NODE_INVALID_HEAD;

    do {
        if (iter_node->picture_type == picture_type && !strcmp(iter_node->description, description)) {
            // node is not head, either joins the previous node to the next node or sets the previous node's next to NULL
            if (iter_node_prev != NULL)
                iter_node_prev->next = iter_node->next;
            // node is head, make the next node the new head
            else
                *head = iter_node->next;

            _free_picture_tag_node(iter_node);

            return NODE_DELETE_SUCCESS;
        }

        iter_node_prev = iter_node;
        iter_node = iter_node->next;
    } while (iter_node != NULL);

    return NODE_NOT_FOUND;
}

/*
 * Frees memory of an entire picture tag linked list based on a pointer to a head pointer.
 * - The original pointer will then be set to NULL.
 *
 * Usage:
 * id3_picture_tag_node* picture_tag_list = NULL;
 * id3_picture_tag_node_add_update(&picture_tag_list, "image/jpeg", APIC_TYPE_COVER_FRONT, "FRONT", "./folder.jpg", NULL, 0);
 * id3_picture_tag_list_destroy(&picture_tag_list);
 */
void id3_picture_tag_list_destroy(id3_picture_tag_node** head) {
    id3_picture_tag_node* iter_node = *head;
    id3_picture_tag_node* free_node = NULL;

    while (iter_node != NULL) {
        free_node = iter_node;
        iter_node = iter_node->next;

        _free_picture_tag_node(free_node);
    }

    *head = NULL;
}

/*
 * [INTERNAL FUNCTION]
 * Generates UTF-8/16 metadata for a text tag node.
 * - Checks if the tag_value contains multibyte sequences, then populates tag_value_utf16 if necessary.
 * - Also sets is_utf8 to 0 or 1, depending on if multibytes were found.
 * - Sets num_id3_bytes to expected text content size when written to an ID3 header.
 * - If anything other that UTF8_PARSE_SUCCESS is returned, is_utf8 and any 16bit variables will retain their old values.
 *
 * Returns (success): UTF8_PARSE_SUCCESS
 * Returns (failure) (_NODE_TYPE_TEXT): UTF8_PARSE_MALFORMED, UTF16_PARSE_NO_MEM, UTF16_PARSE_UTF8_MALFORMED
 * Returns (failure) (_NODE_TYPE_COMMENT): UTF8_PARSE_MALFORMED, UTF16_PARSE_NO_MEM, UTF16_PARSE_UTF8_MALFORMED
 * Returns (failure) (_NODE_TYPE_APIC): UTF8_PARSE_MALFORMED, UTF16_PARSE_NO_MEM, UTF16_PARSE_UTF8_MALFORMED, NODE_FILE_ERROR
 * Returns (failure) (<other>): NODE_INVALID_TAG_NAME
 */
unsigned int _node_generate_metadata(int node_type, void** node) {
    switch (node_type) {
        case _NODE_TYPE_TEXT:
            return _text_node_generate_metadata((id3_text_tag_node**)node);
        case _NODE_TYPE_COMMENT:
            return _comment_node_generate_metadata((id3_comment_tag_node**)node);
        case _NODE_TYPE_APIC:
            return _picture_node_generate_metadata((id3_picture_tag_node**)node);
        default:
            return NODE_INVALID_TAG_NAME;
    }
}

/*
 * [INTERNAL FUNCTION]
 * Generates UTF-8/16 metadata for a text tag node.
 * - Checks if the tag_value contains multibyte sequences, then populates tag_value_utf16 if necessary.
 * - Also sets is_utf8 to 0 or 1, depending on if multibytes were found.
 * - Sets num_id3_bytes to expected text content size when written to an ID3 header. (https://id3.org/id3v2.3.0#ID3v2_frame_overview, https://id3.org/id3v2.3.0#Text_information_frames)
 * - If anything other that UTF8_PARSE_SUCCESS is returned, is_utf8 and tag_value_utf16 will retain their old values.
 *
 * Returns (success): UTF8_PARSE_SUCCESS
 * Returns (failure): UTF8_PARSE_MALFORMED, UTF16_PARSE_NO_MEM, UTF16_PARSE_UTF8_MALFORMED
 */
unsigned int _text_node_generate_metadata(id3_text_tag_node** node) {
    id3_text_tag_node* node_dereferenced = *node;

    unsigned int has_multibytes = utf8_contains_multibyte_sequence(node_dereferenced->tag_value);

    unsigned int previous_utf8_val = node_dereferenced->is_utf8;
    uint16_t* old_tag_value_utf16 = node_dereferenced->tag_value_utf16;

    if (has_multibytes == -1)
        return UTF8_PARSE_MALFORMED;

    else if (has_multibytes == 1) {
        node_dereferenced->is_utf8 = 1;

        unsigned int utf16_length = 0;

        int utf16_fication_outcome = utf8_to_utf16_le(node_dereferenced->tag_value, &node_dereferenced->tag_value_utf16, &utf16_length);
        if (utf16_fication_outcome != UTF16_PARSE_SUCCESS) {
            node_dereferenced->is_utf8 = previous_utf8_val;
            node_dereferenced->tag_value_utf16 = old_tag_value_utf16;
            return utf16_fication_outcome;
        }

        node_dereferenced->num_id3_bytes = _ENCODING_BYTE_LENGTH + _ENCODING_UNICODE_BOM_LENGTH + (utf16_length * 2) + _ENCODING_UNICODE_NULL_LENGTH;
    } else {
        node_dereferenced->is_utf8 = 0;
        node_dereferenced->tag_value_utf16 = NULL;
        node_dereferenced->num_id3_bytes = _ENCODING_BYTE_LENGTH + strlen(node_dereferenced->tag_value) + _ENCODING_ISO_NULL_LENGTH;
    }

    // cleanup old values once new assignments complete, if previous tag was utf8, resolves to nothing
    free(old_tag_value_utf16);

    return UTF8_PARSE_SUCCESS;
}

/*
 * [INTERNAL FUNCTION]
 * Generates UTF-8/16 metadata for a comment tag node.
 * - Checks if either short_content_description or comment contains multibyte sequences,
 *   then populates short_content_description_utf16 and comment_utf16 if necessary.
 * - Also sets is_utf8 to 0 or 1, depending on if multibytes were found.
 * - Sets num_id3_bytes to expected text content size when written to an ID3 header. (https://id3.org/id3v2.3.0#ID3v2_frame_overview, https://id3.org/id3v2.3.0#Comments)
 * - If anything other that UTF8_PARSE_SUCCESS is returned, is_utf8, short_content_description_utf16 and comment_utf16 will retain their old values.
 *
 * Returns (success): UTF8_PARSE_SUCCESS
 * Returns (failure): UTF8_PARSE_MALFORMED, UTF16_PARSE_NO_MEM, UTF16_PARSE_UTF8_MALFORMED
 */
unsigned int _comment_node_generate_metadata(id3_comment_tag_node** node) {
    id3_comment_tag_node* node_dereferenced = *node;

    unsigned int scd_has_multibytes = utf8_contains_multibyte_sequence(node_dereferenced->short_content_description);
    unsigned int cmt_has_multibytes = utf8_contains_multibyte_sequence(node_dereferenced->comment);

    unsigned int previous_utf8_val = node_dereferenced->is_utf8;
    uint16_t* old_scd_utf16 = node_dereferenced->short_content_description_utf16;
    uint16_t* old_cmt_utf16 = node_dereferenced->comment_utf16;

    if (scd_has_multibytes == -1 || cmt_has_multibytes == -1)
        return UTF8_PARSE_MALFORMED;

    else if (scd_has_multibytes == 1 || cmt_has_multibytes == 1) {
        node_dereferenced->is_utf8 = 1;

        unsigned int scd_len = 0, cmt_len = 0;

        int utf16_fication_outcome = utf8_to_utf16_le(node_dereferenced->short_content_description, &node_dereferenced->short_content_description_utf16, &scd_len);
        if (utf16_fication_outcome != UTF16_PARSE_SUCCESS) {
            node_dereferenced->is_utf8 = previous_utf8_val;
            node_dereferenced->short_content_description_utf16 = old_scd_utf16;
            return utf16_fication_outcome;
        }

        utf16_fication_outcome = utf8_to_utf16_le(node_dereferenced->comment, &node_dereferenced->comment_utf16, &cmt_len);
        if (utf16_fication_outcome != UTF16_PARSE_SUCCESS) {
            free(node_dereferenced->short_content_description_utf16);
            node_dereferenced->is_utf8 = previous_utf8_val;
            node_dereferenced->short_content_description_utf16 = old_scd_utf16;
            node_dereferenced->comment_utf16 = old_cmt_utf16;
            return utf16_fication_outcome;
        }

        node_dereferenced->num_id3_bytes = _ENCODING_BYTE_LENGTH + _COMMENT_LANGUAGE_LENGTH +
                                           _ENCODING_UNICODE_BOM_LENGTH + (scd_len * 2) + _ENCODING_UNICODE_NULL_LENGTH +
                                           _ENCODING_UNICODE_BOM_LENGTH + (cmt_len * 2) + _ENCODING_UNICODE_NULL_LENGTH;
    } else {
        node_dereferenced->is_utf8 = 0;
        node_dereferenced->short_content_description_utf16 = NULL;
        node_dereferenced->comment_utf16 = NULL;
        node_dereferenced->num_id3_bytes = _ENCODING_BYTE_LENGTH + _COMMENT_LANGUAGE_LENGTH +
                                           strlen(node_dereferenced->short_content_description) + _ENCODING_ISO_NULL_LENGTH +
                                           strlen(node_dereferenced->comment) + _ENCODING_ISO_NULL_LENGTH;
    }

    // cleanup old values once new assignments complete, if previous tag was utf8, resolves to nothing
    free(old_scd_utf16);
    free(old_cmt_utf16);

    return UTF8_PARSE_SUCCESS;
}

/*
 * [INTERNAL FUNCTION]
 * Generates UTF-8/16 metadata for a picture tag node.
 * - Checks if description contains multibyte sequences, then populates description_utf16 if necessary.
 * - Also sets is_utf8 to 0 or 1, depending on if multibytes were found.
 * - Sets num_id3_bytes to expected text and picture binary content size when written to an ID3 header. (https://id3.org/id3v2.3.0#ID3v2_frame_overview, https://id3.org/id3v2.3.0#Attached_picture)
 * - If the picture is stored as a file, the function will calculate the size of the file and add it to num_id3_bytes.
 * - If the picture is provided as binary data, the function will add the size of the binary data to num_id3_bytes.
 * - If anything other that UTF8_PARSE_SUCCESS is returned, is_utf8 and description_utf16 will retain their old values.
 *
 * Returns (success): UTF8_PARSE_SUCCESS
 * Returns (failure): UTF8_PARSE_MALFORMED, UTF16_PARSE_NO_MEM, UTF16_PARSE_UTF8_MALFORMED, NODE_FILE_ERROR
 */
unsigned int _picture_node_generate_metadata(id3_picture_tag_node** node) {
    id3_picture_tag_node* node_dereferenced = *node;

    unsigned int has_multibytes = utf8_contains_multibyte_sequence(node_dereferenced->description);

    unsigned int previous_utf8_val = node_dereferenced->is_utf8;
    uint16_t* old_description_utf16 = node_dereferenced->description_utf16;

    if (has_multibytes == -1)
        return UTF8_PARSE_MALFORMED;

    else if (has_multibytes == 1) {
        node_dereferenced->is_utf8 = 1;

        unsigned int utf16_length = 0;

        int utf16_fication_outcome = utf8_to_utf16_le(node_dereferenced->description, &node_dereferenced->description_utf16, &utf16_length);
        if (utf16_fication_outcome != UTF16_PARSE_SUCCESS) {
            node_dereferenced->is_utf8 = previous_utf8_val;
            node_dereferenced->description_utf16 = old_description_utf16;
            return utf16_fication_outcome;
        }

        node_dereferenced->num_id3_bytes = _ENCODING_BYTE_LENGTH + strlen(node_dereferenced->mime_type) +
                                           _ENCODING_ISO_NULL_LENGTH + _ENCODING_APIC_PICTURE_TYPE_LENGTH +
                                           _ENCODING_UNICODE_BOM_LENGTH + (utf16_length * 2) + _ENCODING_UNICODE_NULL_LENGTH;
    } else {
        node_dereferenced->is_utf8 = 0;
        node_dereferenced->description_utf16 = NULL;
        node_dereferenced->num_id3_bytes = _ENCODING_BYTE_LENGTH + strlen(node_dereferenced->mime_type) +
                                           _ENCODING_ISO_NULL_LENGTH + _ENCODING_APIC_PICTURE_TYPE_LENGTH +
                                           strlen(node_dereferenced->description) + _ENCODING_ISO_NULL_LENGTH;
    }

    // note: since we are passing binary data as a pointer, do not nullify it like the tags, let programmer settle their own image buffer
    if (node_dereferenced->is_picture_stored_as_file) {
        FILE* fptr;
        fptr = fopen(node_dereferenced->picture_file_path, "rb");

        if (fptr == NULL) {
            node_dereferenced->is_utf8 = previous_utf8_val;
            node_dereferenced->description_utf16 = old_description_utf16;
            return NODE_FILE_ERROR;
        }

        fseek(fptr, 0, SEEK_END);
        node_dereferenced->num_id3_bytes += ftell(fptr);
        fclose(fptr);
    } else {
        node_dereferenced->num_id3_bytes += node_dereferenced->picture_binary_data_bytes;
    }

    // cleanup old values once new assignments complete, if previous tag was utf8, resolves to nothing
    free(old_description_utf16);

    return UTF8_PARSE_SUCCESS;
}

/*
 * [INTERNAL FUNCTION]
 * This function frees a text tag node.
 */
void _free_text_tag_node(id3_text_tag_node* node) {
    free(node->tag_value);
    free(node->tag_value_utf16);
    free(node);
}

/*
 * [INTERNAL FUNCTION]
 * This function frees a comment tag node.
 */
void _free_comment_tag_node(id3_comment_tag_node* node) {
    free(node->short_content_description);
    free(node->comment);
    free(node->short_content_description_utf16);
    free(node->comment_utf16);
    free(node);
}

/*
 * [INTERNAL FUNCTION]
 * This function frees a picture tag node.
 */
void _free_picture_tag_node(id3_picture_tag_node* node) {
    free(node->mime_type);
    free(node->description);
    free(node->description_utf16);
    free(node->picture_file_path);
    free(node->picture_binary_data);
    free(node);
}
