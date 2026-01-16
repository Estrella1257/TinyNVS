#ifndef TINYNVS_H
#define TINYNVS_H

#include <stdint.h>
#include "tinynvs_def.h"

int nvs_format_sector(uint32_t sector_addr, uint32_t old_erase_count, uint32_t seq_id);
int nvs_change_sector_state(uint32_t sector_addr, nvs_sector_state_t new_state);
int nvs_append_entry(uint32_t sector_addr, uint32_t current_offset, const char *key, const void *data, uint16_t len);
int nvs_read_value(uint32_t sector_addr, const char *key, void *out_buf, size_t buf_len);
uint32_t nvs_index_find(const char *key);
void nvs_index_update(const char *key, uint32_t offset);
uint32_t nvs_mount(uint32_t sector_addr);
void nvs_index_clear(void);
uint32_t nvs_gc_collect(uint32_t src_sector, uint32_t dst_sector);
void nvs_index_remove(const char *key);
int nvs_delete(uint32_t sector_addr, const char *key);

int nvs_init(void);

#endif