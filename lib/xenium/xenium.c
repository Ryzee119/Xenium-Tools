#include <string.h>
#include <xboxkrnl/xboxkrnl.h>
#include "xenium.h"

// See https://github.com/Ryzee119/OpenXenium/blob/master/Firmware/openxenium.vhd
// for Xenium CPLD details


static void _io_output(uint16_t address, uint8_t value) {
    __asm__ __volatile__("outb %b0,%w1" : : "a"(value), "Nd"(address));
}

static uint8_t _io_input(uint16_t address) {
    unsigned char v;
    __asm__ __volatile__("inb %w1,%0" : "=a"(v) : "Nd"(address));
    return v;
}

// IO CONTROL INTERFACE
static inline void xenium_set_bank(uint8_t bank) {
    uint8_t reg;
    // Double read/write for extra sanity
    reg = _io_input(XENIUM_REGISTER_BANKING);
    reg = _io_input(XENIUM_REGISTER_BANKING);
    reg &= 0xF0;
    reg |= bank & 0x0F;
    _io_output(XENIUM_REGISTER_BANKING, reg);
    _io_output(XENIUM_REGISTER_BANKING, reg);
}

static inline uint8_t xenium_get_bank() {
    return _io_input(XENIUM_REGISTER_BANKING) & 0x0F;
}

// FLASH MEMORY INTERFACE
static inline void lpc_send_byte(uint32_t address, uint8_t data) {
    volatile uint8_t *lpc_mem_map = (volatile uint8_t *)LPC_MEMORY_BASE;
    lpc_mem_map[address] = data;
}

static inline uint8_t xenium_flash_read_byte(uint32_t address) {
    volatile uint8_t *lpc_mem_map = (volatile uint8_t *)LPC_MEMORY_BASE;
    return lpc_mem_map[address];
}

static inline void xenium_busy_wait(uint8_t address) {
    volatile uint8_t dummy;
    while ((dummy = xenium_flash_read_byte(address)) != xenium_flash_read_byte(address))
        ;
}

static inline void xenium_reset(void) {
    lpc_send_byte(0x0000, 0xF0);
}

static void xenium_sector_erase(uint32_t address) {
    // Address is any address within the sector to erase
    xenium_reset();
    lpc_send_byte(0xAAA, 0xAA);
    lpc_send_byte(0x555, 0x55);
    lpc_send_byte(0xAAA, 0x80);
    lpc_send_byte(0xAAA, 0xAA);
    lpc_send_byte(0x555, 0x55);
    lpc_send_byte(address, 0x30);
}

static uint8_t xenium_address_to_bank(uint32_t address) {
    if (address < 0x100000) return XENIUM_BANK_1_1024;      // 0x000000 to 0x0FFFFF
    if (address < 0x180000) return XENIUM_BANK_XENIUMOS;    // 0x100000 to 0x17FFFF
    if (address < 0x1C0000) return XENIUM_BANK_BOOTLOADER;  // 0x180000 to 0x1BFFFF
    return XENIUM_BANK_RECOVERY;                            // 0x1C0000 to 0xFFFFFF
}

static uint8_t xenium_address_to_sector(uint32_t address) {
    if (address <= 0x1EFFFF) return address / XENIUM_FLASH_SECTOR_SIZE;
    if (address <= 0x1F7FFF) return 31;
    if (address <= 0x1F9FFF) return 32;
    if (address <= 0x1FBFFF) return 33;
    if (address <= 0x1FFFFF) return 34;
    return 34;
}

void xenium_set_led(uint8_t led) {
    _io_output(XENIUM_REGISTER_LED, led);
}

void xenium_flash_read_stream(uint32_t address, uint8_t *data, uint32_t len) {
    volatile uint8_t *volatile lpc_mem_map = (uint8_t *)LPC_MEMORY_BASE;
    uint8_t bank = xenium_get_bank();
    uint8_t original_bank = bank;

    for (uint32_t i = 0; i < len; i++) {
        uint32_t read_address = address + i;

        // Make sure we are on the right bank
        uint8_t wanted_bank = xenium_address_to_bank(read_address);
        if (wanted_bank != bank) {
            bank = wanted_bank;
            xenium_set_bank(wanted_bank);
        }
        data[i] = lpc_mem_map[read_address];
    }
    xenium_set_bank(original_bank);
}

bool xenium_flash_program_stream(uint32_t address, uint8_t *data, uint32_t *len, uint32_t *error_count) {
    bool res = true;

    uint8_t sector = 0xFF;
    uint8_t bank = xenium_get_bank();

    uint32_t wl = *len;    

    for (uint32_t i = 0; i < wl; i++) {
        *len = *len - 1;
        uint32_t write_address = address + i;

        // Erased bytes are already 0xFF, no need to write again
        if (data[i] == 0xFF) {
            continue;
        }

        // Make sure we are on the right bank
        uint8_t wanted_bank = xenium_address_to_bank(write_address);
        if (wanted_bank != bank) {
            bank = wanted_bank;
            xenium_set_bank(wanted_bank);
        }

        // If the sector has changed, we need to erase it before writing.
        uint8_t wanted_sector = xenium_address_to_sector(write_address);
        if (wanted_sector != sector) {
            sector = wanted_sector;
            xenium_sector_erase(write_address);
            xenium_busy_wait(write_address);
        }

        // Write the data
        lpc_send_byte(0xAAA, 0xAA);
        lpc_send_byte(0x555, 0x55);
        lpc_send_byte(0xAAA, 0xA0);
        lpc_send_byte(write_address, data[i]);
        xenium_busy_wait(write_address);

        // Verify the data
        // On a legacy xodus xenium these sectors are protected, so don't worry about verifying this data
        // FIXME, only skip on a legacy xenium
        if (sector == 28 || sector == 29 || sector == 32) {
            continue;
        }

        uint8_t read_back = xenium_flash_read_byte(write_address);
        if (read_back != data[i]) {
            *error_count = *error_count + 1;
            res = false;
        }
    }
    xenium_reset();
    xenium_set_bank(XENIUM_BANK_BOOTLOADER);
    return res;
}

void xenium_chip_ids(uint8_t *manufacturer_id, uint8_t *device_id) {
    // A legacy xodus xenium needs something similar to a real Xos boot process to start working
    // Must be something quirky in the CPLD. This seems to work:
    unsigned char temp[8];
    uint8_t original_bank = xenium_get_bank();
    xenium_flash_read_stream(0x70, temp, 8);
    _io_output(XENIUM_REGISTER_BANKING, XENIUM_BANK_XENIUMOS);
    xenium_flash_read_stream(0x70, temp, 8);
    _io_output(XENIUM_REGISTER_BANKING, XENIUM_BANK_RECOVERY);
    xenium_flash_read_stream(0x70, temp, 8);
    _io_output(XENIUM_REGISTER_BANKING, XENIUM_BANK_BOOTLOADER);

    xenium_reset();
    lpc_send_byte(0xAAA, 0xAA);
    lpc_send_byte(0x555, 0x55);
    lpc_send_byte(0xAAA, 0x90);
    *manufacturer_id = 0x00;
    *device_id = 0x00;
    *manufacturer_id = xenium_flash_read_byte(0x00);
    *device_id = xenium_flash_read_byte(0x02);
    xenium_set_bank(original_bank);
}
