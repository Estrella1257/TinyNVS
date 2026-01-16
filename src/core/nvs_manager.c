#include <stdio.h>
#include "hal_flash.h"
#include "tinynvs.h"

nvs_manager_t g_nvs = {0};

static int get_sector_idx(uint32_t addr) {
    return (addr - NVS_BASE_ADDR) / NVS_SECTOR_SIZE;
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

                best_sector_addr = sector_addr;
                max_seq_id = header.seq_id;
            }
            else {
                printf("  -> Found stale sector! Erasing it at 0x%08X\n", sector_addr);
                hal_flash_erase(sector_addr);
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

uint32_t nvs_get_best_free_sector(void) {
    uint32_t best_addr = 0xFFFFFFFF;
    uint32_t min_erase_count = 0xFFFFFFFF;
    int current_active_idx = get_sector_idx(g_nvs.active_sector_addr);

    for (int i = 0; i < NVS_SECTOR_COUNT; i++) {
        if (i == current_active_idx) continue;

        if (g_nvs.sector_erase_counts[i] < min_erase_count) {
            min_erase_count = g_nvs.sector_erase_counts[i];
            best_addr = NVS_BASE_ADDR + (i * NVS_SECTOR_SIZE);
        }
    }

    if (best_addr == 0xFFFFFFFF) {
        printf("[NVS] Error: No free sector available!\n");
        return 0;
    }
    printf("[WL] Selected Sector %d (EraseCount: %d) as GC Target\n", get_sector_idx(best_addr), min_erase_count);

    return best_addr;
}