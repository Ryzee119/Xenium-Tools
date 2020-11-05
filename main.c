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

#include <stdlib.h>
#include <stdio.h>
#include "lv_conf.h"
#include "lvgl.h"
#include "lv_sdl_drv_display.h"
#include "lv_sdl_drv_input.h"
#include "lv_if_drv_filesystem.h"
#include "menu.h"

#ifdef NXDK
#include <hal/video.h>
#include <hal/debug.h>
#include <windows.h>
//#define printf(fmt, ...) debugPrint(fmt, __VA_ARGS__)
#endif

static const char *fs_id = "0:";

int main(void)
{
    short width = 640;
    short height = 480;
#ifdef NXDK
    int dwEnc = XVideoGetEncoderSettings();
    int dwAdapter = dwEnc & 0x000000FF;
    if (dwEnc & VIDEO_MODE_720P && dwAdapter == AV_PACK_HDTV)
    {
        width = 1280;
        height = 720;
    }
    else
    {
        width = 640;
        height = 480;
    }

    XVideoSetMode(width, height, LV_COLOR_DEPTH, REFRESH_DEFAULT);
#endif

    lv_init();
    lv_sdl_init_display("Xenium-Tools", width, height);
    lv_sdl_init_input();

    lv_if_init_filesystem(fs_id);

    create_menu();

    while (!get_quit_event())
    {
        lv_task_handler();
    }

    lv_sdl_deinit_input();
    lv_sdl_deinit_display();
    lv_if_deinit_filesystem(fs_id);
    lv_deinit();
    return 0;
}
