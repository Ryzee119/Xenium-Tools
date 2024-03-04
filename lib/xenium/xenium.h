#ifndef _XENIUM_H_
#define _XENIUM_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef NXDK
#define LPC_MEMORY_BASE 0xFF000000u
#endif

#define XENIUM_REGISTER_BANKING 0x00EF
#define XENIUM_REGISTER_LED 0x00EE

#define XENIUM_LED_OFF    0
#define XENIUM_LED_RED    1
#define XENIUM_LED_GREEN  2
#define XENIUM_LED_AMBER  3
#define XENIUM_LED_BLUE   4
#define XENIUM_LED_PURPLE 5
#define XENIUM_LED_TEAL   6
#define XENIUM_LED_WHITE  7

#define XENIUM_MANUF_ID  0x01
#define XENIUM_MANUF_ID1  0xC2
#define XENIUM_DEVICE_ID 0xC4
#define XENIUM_FLASH_NUM_SECTORS 35
#define XENIUM_FLASH_SECTOR_SIZE (0x10000)
#define XENIUM_FLASH_SIZE (1024 * 1024 * 2)
#define XENIUM_FLASH_UPDATE_FILE_SIZE (1050964)

#define XENIUM_BANK_TSOP        0
#define XENIUM_BANK_BOOTLOADER  1
#define XENIUM_BANK_XENIUMOS    2
#define XENIUM_BANK_1           3
#define XENIUM_BANK_2           4
#define XENIUM_BANK_3           5
#define XENIUM_BANK_4           6
#define XENIUM_BANK_1_512       7
#define XENIUM_BANK_2_512       8
#define XENIUM_BANK_1_1024      9
#define XENIUM_BANK_RECOVERY    10

void xenium_set_led(uint8_t led);
void xenium_flash_read_stream(uint32_t address, uint8_t *data, uint32_t len);
bool xenium_flash_program_stream(uint32_t address, uint8_t *data, uint32_t *len, uint32_t *error_count);
void xenium_chip_ids(uint8_t *manufacturer_id, uint8_t *device_id);

#endif
