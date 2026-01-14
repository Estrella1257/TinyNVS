#ifndef HAL_FLSAH_H
#define HAL_FLSAH_H

#include <stdint.h>
#include <stddef.h>

#define FLASH_SECTOR_SIZE 4096
#define FLASH_PAGE_SIZE 256
#define FLASH_TOTAL_SIZE  (1024 * 1024)

int hal_flash_init(void);
int hal_flash_read(uint32_t addr, void *buf, size_t len);
int hal_flash_write(uint32_t addr,const void *buf, size_t len);
int hal_flash_erase(uint32_t sector_addr); 

#endif