/* MIT License
 * 
 * Copyright (c) [2020] [Ryan Wendland]
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _MENU_H
#define _MENU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_conf.h"
#include "lvgl.h"

void create_menu(void);

enum xenium_sector_actions
{
    WRITE_SECTOR = 0x0000,
    READ_SECTOR  = 0x8000,
    SAVE_RAWFLASH_TO_FILE = 0x9000,
    READ_RAWFLASH_FROM_FILE = 0xA000,
    SELECT_FILE = 0xB000,
    READ_RECOVERY_FLASH_FROM_FILE = 0xC000,
};

enum xenium_state
{
    FLASH_READING,
    FLASH_WRITING,
    FILE_IO,
    FINISHED,
    FINISHED_WITH_ERROR
};

#ifdef __cplusplus
}
#endif

#endif
