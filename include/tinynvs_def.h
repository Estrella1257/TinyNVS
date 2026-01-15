#ifndef TINYNVS_DEF_H
#define TINYNVS_DEF_H

#include <stdio.h>
#include "hal_flash.h"

#define NVS_MAGIC           0x31564B54
#define NVS_SECTOR_SIZE     4096
#define NVS_BUCKET_SIZE     16

typedef enum {
    SECTOR_STATE_EMPTY = 0xFFFFFFFF,
    SECTOR_STATE_USED = 0xFFFF0000,
    SECTOR_STATE_FULL = 0x00FF0000,
    SECTOR_STATE_GARBAGE = 0x00000000        //扇区数据已废弃，等待回收
} nvs_sector_state_t;

typedef struct {
    uint32_t magic;                         //固定标识
    uint32_t erase_count;                   //擦除计数（用于磨损平衡）
    uint32_t state;
    uint32_t reserved;                      //保留 / 版本号 / CRC校验
} nvs_sector_header_t;

typedef enum {
    ENTRY_STATE_EMPTY = 0xFFFFFFFF,
    ENTRY_STATE_VALID = 0xFFFF0000,          //有效数据
    ENTRY_STATE_DELETED = 0x00000000
} nvs_entry_state_t;

typedef struct {
    uint8_t key_len;
    uint8_t type;
    uint16_t data_len;
    uint32_t crc;
    uint32_t state;
} nvs_entry_header_t;

#define ALIGN_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))

//一个Entry的实际大小: 头部 + key长度 + data长度 + 对齐
#define NVS_ENTRY_SIZE(k_ken, d_len) \
    (sizeof(nvs_entry_header_t) + ALIGN_UP((k_len) + (d_len), 4))

typedef struct nvs_index_node {
    uint32_t key_hash;
    uint32_t offset;
    struct nvs_index_node *next;
} nvs_index_node_t;

#endif