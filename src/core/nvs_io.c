#include <string.h>
#include "hal_flash.h"
#include "tinynvs.h"
#include "crc32.h"

int nvs_append_entry(uint32_t sector_addr, uint32_t current_offset, const char *key, const void* data, uint16_t len) {
    uint8_t key_len = strlen(key);

    //对齐后的总大小
    uint32_t payload_len = key_len + len;
    uint32_t total_size = sizeof(nvs_entry_header_t) + ALIGN_UP(payload_len, 4);

    if (current_offset + total_size > NVS_SECTOR_SIZE) {
        return -1;      // 返回满，交给上层处理 GC
    }

    uint32_t write_addr = sector_addr + current_offset;

    uint32_t check_crc = crc32_init();
    check_crc = crc32_update(check_crc, key, key_len);
    check_crc = crc32_update(check_crc, data, len);
    check_crc = crc32_final(check_crc);

    nvs_entry_header_t header;
    header.key_len = key_len;
    header.type = 0;
    header.data_len = len;
    header.crc = check_crc;
    header.state = ENTRY_STATE_VALID;

    uint32_t payload_addr = write_addr + sizeof(header);

    // 关键顺序：先写内容，后写头
    // 这样如果写内容时断电，Header 还是 0xFF，下次扫描会忽略这块区域
    hal_flash_write(payload_addr, key , key_len);
    hal_flash_write(payload_addr + key_len, data, len);
    hal_flash_write(write_addr, &header, sizeof(header));

    return current_offset + total_size;
}

int nvs_set(const char *key, const void *data,uint16_t len) {
    if (key == NULL || data == NULL || len == 0) return -1;
    if (strlen(key) > NVS_KEY_MAX_LEN) return -2;

    // 1. 获取写入前的 offset (这就是该数据存放的起始地址)
    uint32_t item_offset = g_nvs.write_offset;

    // 2. 第一次尝试写入
    int next_offset = nvs_append_entry(g_nvs.active_sector_addr, item_offset, key, data, len);

    // 3. 如果扇区满了 (返回 -1)，执行 GC
    if (next_offset < 0) {
        printf("[NVS] Sector full, triggering GC...\n");

        // 执行垃圾回收 (数据搬运 -> 切换扇区)
        if (nvs_execute_gc() != 0) {
            printf("[NVS] GC Failed! Flash might be full or broken.\n");
            return -3;   //致命错误
        }

        // 获取新的 offset
        item_offset = g_nvs.write_offset;

        // 4. 第二次尝试写入 (写到新的 active sector)
        next_offset = nvs_append_entry(g_nvs.active_sector_addr, item_offset, key, data, len);

        if (next_offset < 0) {
            printf("[NVS] Error: Storage full even after GC!\n");
            return -4; // 数据太大，或者是真的存满了
        }
    }

    // 5. 写入成功，更新 RAM 索引：将 Key 指向刚才写入的 item_offset
    nvs_index_update(key, item_offset);

    // 更新全局写入指针，指向下一个空闲位置
    g_nvs.write_offset = (uint32_t)next_offset;

    return 0;
}

int nvs_get(const char *key, void *buf, uint16_t len) {
    if (key == NULL || buf == NULL) return -1;
    
    // 1. 在 RAM 索引中查找 Key
    uint32_t offset = nvs_index_find(key);

    if (offset == 0) return -1; // 没找到

    // 【关键点 1】算出绝对物理地址
    uint32_t addr = g_nvs.active_sector_addr + offset; 
    
    nvs_entry_header_t header;

    // 2. 读取 Entry 头部
    // 【修正】直接用 addr，不要再加 offset
    hal_flash_read(addr, &header, sizeof(header)); 

    // 3. 校验数据有效性
    if (header.state != ENTRY_STATE_VALID) return -2;

    // 4. 检查用户缓冲区是否够大
    if (len < header.data_len) {
        return -3;
    }

    // --- CRC 校验逻辑 ---
    uint32_t calc_crc = crc32_init();
    
    // 【修正】payload 地址紧跟在 header 后面
    uint32_t payload_addr = addr + sizeof(header); 

    // 假设你的 Key 最大长度不会爆栈，这里用 256 字节的临时 buffer 是可以的
    // 但更安全的做法是分段读或者确认 Key 长度限制
    char key_temp[256]; 
    if (header.key_len > sizeof(key_temp)) return -4; // 安全检查

    hal_flash_read(payload_addr, key_temp, header.key_len);
    calc_crc = crc32_update(calc_crc, key_temp, header.key_len);

    hal_flash_read(payload_addr + header.key_len, buf, header.data_len);
    calc_crc = crc32_update(calc_crc, buf, header.data_len);

    calc_crc = crc32_final(calc_crc);

    if (calc_crc != header.crc) {
        return -2; // CRC 校验失败
    }
    
    return header.data_len;
}

int nvs_delete(uint32_t sector_addr, const char *key) {
    uint32_t offset = nvs_index_find(key);

    if (offset == 0) {
        return -1;         //根本不存在,没法删
    }

    uint32_t state_offset_in_header = offsetof(nvs_entry_header_t, state);
    uint32_t flash_write_addr = sector_addr + offset + state_offset_in_header;

    nvs_entry_state_t del_state = ENTRY_STATE_DELETED;

    int ret = hal_flash_write(flash_write_addr, &del_state, sizeof(del_state));
    if (ret != 0) {
        return -2;         //硬件写入失败
    }

    nvs_index_remove(key);

    return 0;
}