#ifndef TINYNVS_DEF_H
#define TINYNVS_DEF_H

#include <stdio.h>
#include "hal_flash.h"

#define NVS_MAGIC           0x31564B54
#define NVS_SECTOR_SIZE     4096

typedef enum {
    SECTOR_STATE_EMPTY = 0xFFFFFFFF,
    SECTOR_STATE_USED = 0xFFFF0000,
    SECTOR_STATE_FULL = 0x00FF0000,
    SECTOR_STATE_GARBAGE = 0x00000000
} nvs_sector_state_t;

typedef struct {
    uint32_t magic;
    uint32_t erase_count;
    uint32_t state;
    uint32_t reserved;
} nvs_sector_header_t;

#endif