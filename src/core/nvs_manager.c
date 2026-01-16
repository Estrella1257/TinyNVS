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
    return (header.state == SECTOR_STATE_USED || header.state == SECTOR_STATE_FULL);
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
        if (header.state != SECTOR_STATE_USED && header.state != SECTOR_STATE_FULL) continue; 

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

                int old_idx = get_sector_idx(best_sector_addr);
                g_nvs.sector_erase_counts[old_idx]++;

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

        nvs_index_clear();
        
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

uint32_t nvs_get_best_free_sector(uint32_t exclude_addr) {
    uint32_t best_addr = 0xFFFFFFFF;
    uint32_t min_erase_count = 0xFFFFFFFF;

    for (int i = 0; i < NVS_SECTOR_COUNT; i++) {
        uint32_t current_addr = NVS_BASE_ADDR + (i * NVS_SECTOR_SIZE);
        
        if (current_addr == g_nvs.active_sector_addr) continue;

        if (current_addr == exclude_addr) continue;

        if (is_sector_used(current_addr)) continue;

        if (g_nvs.sector_erase_counts[i] < min_erase_count) {
            min_erase_count = g_nvs.sector_erase_counts[i];
            best_addr = NVS_BASE_ADDR + (i * NVS_SECTOR_SIZE);
        }
    }

    if (best_addr != 0) {
        printf("[Manager] Selected Best Free Sector: 0x%08X (EraseCount: %d)\n", best_addr, min_erase_count);
    }
    else {
        printf("[NVS] Error: No free sector available!\n");
    }
    
    return best_addr;
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
        printf("[WL-Static] Threshold exceeded! Forcing migration of Cold Sector %d...\n", min_idx);

        uint32_t cold_sector_addr = NVS_BASE_ADDR + min_idx * NVS_SECTOR_SIZE;

        uint32_t target_sector = nvs_get_best_free_sector(cold_sector_addr);
        if (target_sector == 0) return 0;

        nvs_gc_collect(cold_sector_addr, target_sector);

        int target_idx = (target_sector - NVS_BASE_ADDR) / 4096;
        g_nvs.sector_erase_counts[target_idx]++;
        g_nvs.sector_erase_counts[min_idx]++;

        printf("[WL-Static] Migration Done. Old Cold Sector %d is now Free.\n", min_idx);
        return 1;
    }
    return 0;
}