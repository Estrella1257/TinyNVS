#include <stdio.h>
#include "hal_flash.h"
#include "tinynvs.h"

nvs_manager_t g_nvs = {0};

static int get_sector_idx(uint32_t addr) {
    return (addr - NVS_BASE_ADDR) / NVS_SECTOR_SIZE;
}

static int is_sector_used(uint32_t sector_addr) {
    nvs_sector_header_t header;
    hal_flash_read(sector_addr, &header, sizeof(header));
    return (header.state == SECTOR_STATE_USED);
}

static uint32_t nvs_get_best_free_sector(void) {
    uint32_t best_addr = 0;
    uint32_t min_erase_count = 0xFFFFFFFF;
    int found = 0;

    for (int i = 0; i < NVS_SECTOR_COUNT; i++) {
        uint32_t current_addr = NVS_BASE_ADDR + (i * NVS_SECTOR_SIZE);
        
        if (current_addr == g_nvs.active_sector_addr) continue;

        if (g_nvs.sector_erase_counts[i] < min_erase_count) {
            min_erase_count = g_nvs.sector_erase_counts[i];
            best_addr = current_addr;
            found = 1;
        }
    }

    if (found) {
        printf("[Manager] Selected Best Free Sector: 0x%08X (EraseCount: %d)\n", best_addr, min_erase_count);
        return best_addr;
    }
    
    return 0;
}

int nvs_execute_gc(void) {
    uint32_t src_sector = g_nvs.active_sector_addr;
    uint32_t dst_sector = nvs_get_best_free_sector();

    if (dst_sector == 0) {
        printf("[GC] Error: No free sector available!\n");
        return -1;
    }

    printf("[GC] Start: 0x%X -> 0x%X\n", src_sector, dst_sector);

    // 1. 擦除目标扇区 (确保干净)
    hal_flash_erase(dst_sector);

    // 2. 写入头部，状态标记为 COPYING (中间态)
    //    如果此时掉电，下次 init 会发现这个扇区是 COPYING，说明是垃圾数据
    nvs_sector_header_t new_header;
    new_header.magic = NVS_MAGIC;
    new_header.seq_id = g_nvs.current_seq_id + 1;
    new_header.state = SECTOR_STATE_COPYING;
    new_header.erase_count = g_nvs.sector_erase_counts[get_sector_idx(dst_sector)];

    hal_flash_write(dst_sector, &new_header, sizeof(new_header));

    // 3. 搬运数据 (调用 index.c 中的函数)
    //    这一步会更新 RAM 中的索引指向新地址
    uint32_t new_write_offset = nvs_index_gc_copy_data(src_sector, dst_sector);

    if (new_write_offset == 0) {
        printf("[GC] Copy failed (Sector full or Error).\n");
        // 恢复 RAM 索引可能很复杂，建议重启系统或者在此处处理回滚
        return -2;
    }

    // 4. 搬运成功，将新扇区标记为 USED (正式生效)
    //    这个状态切换是原子性的commit点
    nvs_change_sector_state(dst_sector, SECTOR_STATE_USED);

    // 5. 擦除旧扇区
    hal_flash_erase(src_sector);
    g_nvs.sector_erase_counts[get_sector_idx(src_sector)]++;

    // 6. 更新全局管理器状态
    g_nvs.active_sector_addr = dst_sector;
    g_nvs.write_offset = new_write_offset;
    g_nvs.current_seq_id++;

    printf("[GC] Done. New Active: 0x%X\n", dst_sector);
    return 0;
}

int nvs_init(void) {
    nvs_sector_header_t header;

    uint32_t best_sector_addr = 0xFFFFFFFF;
    uint32_t max_seq_id = 0;
    int found_candidate = 0;

    printf("[NVS] Init: Scaning %d sectors...\n", NVS_SECTOR_COUNT);

    // 1. 遍历所有扇区
    for (int i = 0; i < NVS_SECTOR_COUNT; i++) {
        uint32_t sector_addr = NVS_BASE_ADDR + (i * NVS_SECTOR_SIZE);

        hal_flash_read(sector_addr, &header, sizeof(header));

        if (header.magic == NVS_MAGIC) {
            g_nvs.sector_erase_counts[i] = header.erase_count;
        }
        else {
            g_nvs.sector_erase_counts[i] = 0;
        }

        // 检查 Magic Number 是否合法
        if (header.magic != NVS_MAGIC) continue;       
        
        if (header.state == SECTOR_STATE_COPYING) {
            printf("  -> Found interrupted GC sector at 0x%08X. Erasing.\n", sector_addr);
            hal_flash_erase(sector_addr);
            g_nvs.sector_erase_counts[i]++;
            continue;
        }

        if (header.state != SECTOR_STATE_USED) continue; 

        printf("  -> Sector at 0x%08X is Active. Seq: %d\n", sector_addr, header.seq_id);

        if (!found_candidate) {
            best_sector_addr = sector_addr;
            max_seq_id = header.seq_id;
            found_candidate = 1;
        }
        else {
            if (header.seq_id > max_seq_id) {
                printf("  -> Found newer sector! Erasing old sector at 0x%08X\n", best_sector_addr);
                hal_flash_erase(best_sector_addr);

                g_nvs.sector_erase_counts[get_sector_idx(best_sector_addr)]++;

                best_sector_addr = sector_addr;
                max_seq_id = header.seq_id;
            }
            else {
                printf("  -> Found stale sector! Erasing it at 0x%08X\n", sector_addr);
                hal_flash_erase(sector_addr);
                g_nvs.sector_erase_counts[i]++;
            }
        }
    }

    if (found_candidate) {
        printf("[NVS] Mounting Best Sector at 0x%08X (Seq: %d)\n", best_sector_addr, max_seq_id);
        
        uint32_t next_offset = nvs_mount(best_sector_addr);

        g_nvs.active_sector_addr = best_sector_addr;
        g_nvs.write_offset = next_offset;
        g_nvs.current_seq_id = max_seq_id;
    }
    else {
        printf("[NVS] No active sector. Formatting Sector 0...\n");
        uint32_t first_sector = NVS_BASE_ADDR;

        nvs_format_sector(first_sector, 1, 1);
        nvs_change_sector_state(first_sector, SECTOR_STATE_USED);

        g_nvs.active_sector_addr = first_sector;
        g_nvs.write_offset = sizeof(nvs_sector_header_t);
        g_nvs.current_seq_id = 1;

        g_nvs.sector_erase_counts[0] = 1;

        nvs_index_clear();
    }
    return 0;
}

int nvs_check_and_execute_static_wl(void) {
    uint32_t max_count = 0;
    uint32_t min_count = 0xFFFFFFFF;
    int min_idx = -1;

    for (int i = 0; i < NVS_SECTOR_COUNT; i++) {
        uint32_t cnt = g_nvs.sector_erase_counts[i];
        if (cnt > max_count) max_count = cnt;

        uint32_t addr = NVS_BASE_ADDR + i * NVS_SECTOR_SIZE;

        if (addr != g_nvs.active_sector_addr && is_sector_used(addr)) {
            if (cnt < min_count) {
                min_count = cnt;
                min_idx = i;
            }
        }
    }

    if (min_idx == -1) return 0;

    uint32_t diff = (max_count > min_count) ? (max_count - min_count) : 0;

    printf("[WL-Static] Check: Max=%d, Min=%d (Sector %d), Diff=%d\n", max_count, min_count, min_idx, diff);

    if (diff > NVS_STATIC_WL_THRESHOLD) {
        printf("[WL-Static] Threshold exceeded! Forcing GC...\n");

        if (nvs_execute_gc() == 0) {
            return 1;
        }
    }
    return 0;
}