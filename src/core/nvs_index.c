#include <stdlib.h>
#include <string.h>
#include "tinynvs.h"
#include "crc32.h"
#include "hal_flash.h"

static nvs_index_node_t *buckets[NVS_BUCKET_SIZE] = {NULL};

static uint8_t get_bucket_idx(uint32_t hash) {
    return hash % NVS_BUCKET_SIZE;
}

void nvs_index_update(const char *key, uint32_t offset) {
    uint32_t hash = crc32_compute(key, strlen(key));
    uint8_t idx = get_bucket_idx(hash);

    // A. 查找是否存在 (覆盖旧数据的 offset)
    nvs_index_node_t *node = buckets[idx];
    while (node) {
        if(node->key_hash == hash) {
            node->offset = offset;
            return;
        }
        node = node->next;
    }

    // B. 不存在，新建节点 (头插法)
    nvs_index_node_t *new_node = malloc(sizeof(nvs_index_node_t));
    if (new_node) {
        new_node->key_hash = hash;
        new_node->offset = offset;
        new_node->next = buckets[idx];          // 插入链表头
        buckets[idx] = new_node;                // 把new_node作为buckets的头节点
    }
}
 
uint32_t nvs_index_find(const char *key) {
    uint32_t hash = crc32_compute(key, strlen(key));
    uint8_t idx = get_bucket_idx(hash);

    nvs_index_node_t *node = buckets[idx];
    while (node) {
        if (node->key_hash == hash) {
            return node->offset;
        }
        node = node->next;
    }
    return 0;
}

void nvs_index_clear(void) {
    for (int i = 0; i < NVS_BUCKET_SIZE; i++) {
        nvs_index_node_t *current = buckets[i];
        while (current != NULL) {
            nvs_index_node_t *temp = current;
            current = current->next;
            free(temp);
        }
        buckets[i] = NULL;
    }
}

// --- 3. 挂载 (Mount) - 核心功能 ---
// 扫描整个扇区，重建 RAM 索引，并返回下一个可写入的地址
uint32_t nvs_mount(uint32_t sector_addr) {
    void nvs_index_clear(void);

    uint32_t offset = sizeof(nvs_sector_header_t);
    nvs_entry_header_t header;
    char key_buf[128];

    while (offset < NVS_SECTOR_SIZE) {
        hal_flash_read(sector_addr + offset, &header, sizeof(header));

        if (header.state == ENTRY_STATE_EMPTY) {
            break;
        }

        uint32_t payload_len = ALIGN_UP(header.key_len + header.data_len, 4);
        uint32_t next_offset = offset + sizeof(header) + payload_len;

        if (header.state == ENTRY_STATE_VALID) {
            hal_flash_read(sector_addr + offset + sizeof(header), key_buf, header.key_len);
            key_buf[header.key_len] = '\0';

            nvs_index_update(key_buf, offset);
        }
        offset = next_offset;
    }
    return offset;
}

uint32_t nvs_gc_collect(uint32_t src_sector, uint32_t dst_sector) {
    uint32_t new_seq = g_nvs.current_seq_id + 1;
    
    nvs_format_sector(dst_sector, 1, new_seq);

    uint32_t current_offset = sizeof(nvs_sector_header_t);

    char key_buf[128];
    char data_buf[256];
    nvs_entry_header_t header;

    for (int i = 0; i < NVS_BUCKET_SIZE; i++) {
        nvs_index_node_t *node = buckets[i];

        while (node) {
            uint32_t src_addr = src_sector + node->offset;

            hal_flash_read(src_addr, &header, sizeof(header));
            
            //读key
            hal_flash_read(src_addr + sizeof(header), key_buf, header.key_len);
            key_buf[header.key_len] = '\0';
            
            //读data
            hal_flash_read(src_addr + sizeof(header) + header.key_len, data_buf, header.data_len);

            int new_offset = nvs_append_entry(dst_sector, current_offset, key_buf, data_buf, header.data_len);

            //更新 RAM 索引指向新地址
            node->offset = current_offset;

            //推进偏移量
            current_offset = new_offset;
            node = node->next;
        }
    }

    g_nvs.current_seq_id = new_seq;

    hal_flash_erase(src_sector);

    return current_offset;
}

void nvs_index_remove(const char *key) {
    uint32_t hash = crc32_compute(key, strlen(key));
    uint8_t idx = hash % NVS_BUCKET_SIZE;

    nvs_index_node_t *current = buckets[idx];
    nvs_index_node_t *prev = NULL;

    while (current) {
        if (current->key_hash == hash) {
            if (prev == NULL) {
                buckets[idx] = current->next;
            }
            else {
                prev->next = current->next;
            }

            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}