#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "id3_process.h"

#define _TAG_NAME_USER_TEXT "TXXX"
#define _TAG_NAME_COMMENT "COMM"
#define _TAG_NAME_PICTURE "APIC"

void id3_write_tag(char* file_path, id3_master_tag_struct master_tag_collection);
void id3_init_master_tag(id3_master_tag_struct* master_tag_collection);
