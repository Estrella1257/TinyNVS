#include <stdlib.h>
#include <string.h>
#include "tinynvs.h"
#include "crc32.h"
#include "hal_flash.h"

static nvs_index_node_t *buckets[NVS_BUCKET_SIZE] = {NULL};

static nvs_index_node_t *alloc_node(void) {
    for (int i = 0; i < NVS_MAX_KEYS; i++) {
        if (g_nvs.node_pool[i].used == 0) {
            g_nvs.node_pool[i].used = 1;
            g_nvs.node_pool[i].next = NULL;
            return &g_nvs.node_pool[i];
        }
    }
    return NULL;   //内存池满了
}

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
    nvs_index_node_t *new_node = alloc_node();
    if (new_node) {
        new_node->key_hash = hash;
        new_node->offset = offset;
        new_node->next = buckets[idx];          // 插入链表头
        buckets[idx] = new_node;                // 把new_node作为buckets的头节点
    }
    else {
        printf("too many keys\n");
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
        buckets[i] = NULL;
    }
    for (int i = 0; i < NVS_MAX_KEYS; i++) {
        g_nvs.node_pool[i].used = 0;
    }
}

// --- 3. 挂载 (Mount) - 核心功能 ---
// 扫描整个扇区，重建 RAM 索引，并返回下一个可写入的地址
uint32_t nvs_mount(uint32_t sector_addr) {
    nvs_index_clear();

    uint32_t offset = sizeof(nvs_sector_header_t);
    nvs_entry_header_t header;
    char key_buf[NVS_KEY_MAX_LEN];

    while (offset < NVS_SECTOR_SIZE) {
        hal_flash_read(sector_addr + offset, &header, sizeof(header));

        if (header.state == ENTRY_STATE_EMPTY) {
            break;
        }

        uint32_t payload_len = ALIGN_UP(header.key_len + header.data_len, 4);
        uint32_t next_offset = offset + sizeof(header) + payload_len;

        if (next_offset > NVS_SECTOR_SIZE) break; 

        if (header.state == ENTRY_STATE_VALID) {
            hal_flash_read(sector_addr + offset + sizeof(header), key_buf, header.key_len);
            key_buf[header.key_len] = '\0';

            uint32_t calc_crc = crc32_init();
            calc_crc = crc32_update(calc_crc, key_buf, header.key_len);

            // 此时需要读 Data 部分来计算 CRC
            // 为了节省 RAM，可以分块读，或者如果数据小直接读
            char temp_data[64]; // 假设最大数据不超过这个，或者用动态 buffer
            hal_flash_read(sector_addr + offset + sizeof(header) + header.key_len, temp_data, header.data_len);
            calc_crc = crc32_update(calc_crc, temp_data, header.data_len);

            if (crc32_final(calc_crc) == header.crc) {
                nvs_index_update(key_buf, offset);
            } 
            else {
                printf("[NVS] Corrupted entry found at offset %d, skipping.\n", offset);
            }
        }
        offset = next_offset;
    }
    return offset;
}

// [重构] 该函数不再负责擦除扇区，只负责将 RAM 索引指向的数据搬运到目标扇区
// 返回值: 搬运完成后的新 offset，如果出错返回 0
uint32_t nvs_index_gc_copy_data(uint32_t src_sector, uint32_t dst_sector) {
    uint32_t current_offset = sizeof(nvs_sector_header_t);

    char key_buf[NVS_KEY_MAX_LEN];
    char data_buf[NVS_DATA_MAX_LEN];
    nvs_entry_header_t header;

    for (int i = 0; i < NVS_BUCKET_SIZE; i++) {
        nvs_index_node_t *node = buckets[i];

        while (node) {
            uint32_t src_addr = src_sector + node->offset;

            hal_flash_read(src_addr, &header, sizeof(header));

            // 校验数据合法性
            if (header.state != ENTRY_STATE_VALID) {
                node = node->next;
                continue;
            }
            
            //读key
            hal_flash_read(src_addr + sizeof(header), key_buf, header.key_len);
            key_buf[header.key_len] = '\0';
            
            //读data
            hal_flash_read(src_addr + sizeof(header) + header.key_len, data_buf, header.data_len);

            int ret_offset = nvs_append_entry(dst_sector, current_offset, key_buf, data_buf, header.data_len);

            if (ret_offset <= 0) {
                printf("[GC] Error: Destination sector full during copy!\n");
                return 0;       //搬运失败
            }
            //更新 RAM 索引指向新地址
            node->offset = current_offset;

            //推进偏移量
            current_offset = (uint32_t)ret_offset;
            node = node->next;
        }
    }
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

            current->used = 0;
            return;
        }
        prev = current;
        current = current->next;
    }
}