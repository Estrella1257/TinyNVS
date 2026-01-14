#include "hal_flash.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define FLASH_FILE "flash_mock.bin"

static FILE *flash_fp = NULL;

int hal_flash_init(void) {
    flash_fp = fopen(FLASH_FILE, "rb+");
    if (flash_fp == NULL) {
        flash_fp = fopen(FLASH_FILE, "wb+");
        if (flash_fp == NULL) {
            printf("[Mock] Error: Unable to create flash.\n");
            return -1;
        }

        uint8_t sector_buf[FLASH_SECTOR_SIZE];
        memset(sector_buf, 0xFF, FLASH_SECTOR_SIZE);
        for (int i = 0; i < FLASH_TOTAL_SIZE / FLASH_SECTOR_SIZE; i++) {
            fwrite(sector_buf, 1, FLASH_SECTOR_SIZE, flash_fp);
        }
        printf("[Mock] Flash created: %d bytes(All 0xFF)\n", FLASH_TOTAL_SIZE);
    }
    return 0;
}

int hal_flash_read(uint32_t addr, void *buf, size_t len) {
    if (addr + len > FLASH_TOTAL_SIZE) return -1;

    fseek(flash_fp, addr, SEEK_SET);
    size_t read_len = fread(buf, 1, len, flash_fp);
    return (read_len == len) ? 0 : -1;
}

int hal_flash_write(uint32_t addr, const void *buf, size_t len) {
    if (addr + len > FLASH_TOTAL_SIZE) return -1;

    uint8_t *new_data = (uint8_t *)buf;
    uint8_t current_byte;
    int error_flag = 0;

    for (size_t i = 0; i < len; i++) {
        fseek(flash_fp, addr + i, SEEK_SET);
        fread(&current_byte, 1, 1, flash_fp);

        uint8_t final_byte = current_byte & new_data[i];

        if (final_byte != new_data[i]) {
            if (!error_flag) {
                printf("\n[Mock] HARDWARE ERROR at addr 0x%08lX:""Bit flip 0->1 prohibited without erase!\n""        Old:0x%02x, New:0x%02x -> Result: 0x%02X\n", addr + i, current_byte, new_data[i], final_byte);
                error_flag = 1;          
            }
        }
        fseek(flash_fp, addr + i, SEEK_SET);
        fwrite (&final_byte, 1, 1, flash_fp);
    }
    fflush(flash_fp);
    return 0;
}

int hal_flash_erase(uint32_t sector_addr) {
    if (sector_addr % FLASH_SECTOR_SIZE != 0) {
        printf("[Mock] Error: Erase address 0x%X not aligned to sector size!\n", sector_addr);
        return -1;
    }

    if (sector_addr + FLASH_SECTOR_SIZE > FLASH_TOTAL_SIZE) return -1;

    uint8_t sector_buf[FLASH_SECTOR_SIZE];
    memset(sector_buf, 0xFF, FLASH_SECTOR_SIZE);

    fseek(flash_fp, sector_addr, SEEK_SET);
    fwrite(sector_buf, 1, FLASH_SECTOR_SIZE, flash_fp);
    fflush(flash_fp);

    printf("[Mock] Erased sector at 0x%08X\n", sector_addr);
    return 0;
}