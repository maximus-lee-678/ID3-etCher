#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct id3_text_tag_node id3_text_tag_node;
typedef struct id3_comment_tag_node id3_comment_tag_node;
typedef struct id3_picture_tag_node id3_picture_tag_node;
typedef struct id3_master_tag_struct id3_master_tag_struct;

#include "utf.h"
#include "id3_write.h"

// num_id3_bytes will take into account size after UTF8-fication and the null byte at the end.
struct id3_text_tag_node {
    char tag_name[5];
    char* tag_value;
    unsigned int num_id3_bytes;
    int is_utf8;
    uint16_t* tag_value_utf16;

    struct id3_text_tag_node* next;
};

struct id3_comment_tag_node {
    char language[4];
    char* short_content_description;
    char* comment;
    unsigned int num_id3_bytes;
    int is_utf8;
    uint16_t* short_content_description_utf16;
    uint16_t* comment_utf16;

    struct id3_comment_tag_node* next;
};

struct id3_picture_tag_node {
    char* mime_type;
    uint8_t picture_type;
    char* description;
    unsigned int num_id3_bytes;
    int is_utf8;
    uint16_t* description_utf16;
    int is_picture_stored_as_file;
    char* picture_file_path;
    uint8_t* picture_binary_data;
    unsigned int picture_binary_data_bytes;

    struct id3_picture_tag_node* next;
};

struct id3_master_tag_struct {
    id3_text_tag_node** text_tag_list;
    id3_comment_tag_node** comment_tag_list;
    id3_picture_tag_node** picture_tag_list;
};

// has anyone heard of oop?
// i tried to use base linked lists to join them all the a single linked list but it was very messy
// this is easier to maintain, but has redundancy

#define NODE_ADD_SUCCESS 100
#define NODE_UPDATE_SUCCESS 101
#define NODE_DELETE_SUCCESS 102
#define NODE_INVALID_TAG_NAME 103
#define NODE_INVALID_TAG_VALUE 104
#define NODE_INVALID_HEAD 105
#define NODE_NOT_FOUND 106
#define NODE_MEMORY_ERROR 107
#define NODE_FILE_ERROR 108

#define TAG_CREATE_SUCCESS 109
#define TAG_UPDATE_SUCCESS 110
#define TAG_INVALID_VALUE 111

#define APIC_TYPE_OTHER 0x00
#define APIC_TYPE_FILE_ICON 0x01
#define APIC_TYPE_OTHER_FILE_ICON 0x02
#define APIC_TYPE_COVER_FRONT 0x03
#define APIC_TYPE_COVER_BACK 0x04
#define APIC_TYPE_LEAFLET_PAGE 0x05
#define APIC_TYPE_MEDIA 0x06
#define APIC_TYPE_LEAD 0x07
#define APIC_TYPE_ARTIST 0x08
#define APIC_TYPE_CONDUCTOR 0x09
#define APIC_TYPE_BAND_ORCHESTRA 0x0A
#define APIC_TYPE_COMPOSER 0x0B
#define APIC_TYPE_LYRICIST 0x0C
#define APIC_TYPE_RECORDING_LOCATION 0x0D
#define APIC_TYPE_DURING_RECORDING 0x0E
#define APIC_TYPE_DURING_PERFORMANCE 0x0F
#define APIC_TYPE_MOVIE_VIDEO_CAPTURE 0x10
#define APIC_TYPE_POGFISH 0x11
#define APIC_TYPE_ILLUSTRATION 0x12
#define APIC_TYPE_BAND_ARTIST_LOGOTYPE 0x13
#define APIC_TYPE_PUBLISHER_STUDIO_LOGOTYPE 0x14

unsigned int id3_text_tag_node_add_update(id3_text_tag_node** head, char* tag_name, char* tag_value);
unsigned int id3_text_tag_node_delete(id3_text_tag_node** head, char* tag_name);
void id3_text_tag_list_destroy(id3_text_tag_node** head);
unsigned int id3_comment_tag_node_add_update(id3_comment_tag_node** head, char* language, char* short_content_description, char* comment);
unsigned int id3_comment_tag_node_delete(id3_comment_tag_node** head, char* language, char* short_content_description);
void id3_comment_tag_list_destroy(id3_comment_tag_node** head);
unsigned int id3_picture_tag_node_add_update(id3_picture_tag_node** head, char* mime_type, uint8_t picture_type, char* description,char* picture_file_path, uint8_t* picture_binary_data, unsigned int picture_binary_data_bytes);
unsigned int id3_picture_tag_node_delete(id3_picture_tag_node** head, uint8_t picture_type, char* description);
void id3_picture_tag_list_destroy(id3_picture_tag_node** head);
