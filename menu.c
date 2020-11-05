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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "lv_conf.h"
#include "lvgl.h"
#include "menu.h"
#include "lv_sdl_drv_input.h"
#include "lv_if_drv_filesystem.h"
#include "xenium.h"

/* GUI Objects */
static const int XMARGIN = 15;
static const int YMARGIN = 15;
static lv_indev_t *sdl_indev;
static lv_group_t *input_group;
static lv_obj_t *main_list;
static lv_obj_t *status_label;
static lv_obj_t *progress_bar;
static lv_task_t *background_tasks;
static lv_task_t *xenium_tasks;
static lv_obj_t *file_list;
static lv_obj_t *alert_box;
static lv_obj_t *xenium_status;
static lv_obj_t *xenium_status_orb;
static lv_style_t xenium_status_orb_style;

/* Filesystem Objects */
lv_fs_dir_t cwd;
static const char *fs_id = "0:";
static char *fs_create_path(const char *fs_id, const char *path);
static void file_list_cb(lv_obj_t *ta, lv_event_t e);
static char selected_file_name[256] = {0};
#ifdef NXDK
//#define printf(fmt, ...) debugPrint(fmt, __VA_ARGS__)
const char *dir_path = "D:\\";
#else
const char *dir_path = "./";
#endif

/* GUI Callbacks */
static void cb_btn_xenium_rescan(lv_obj_t *ta, lv_event_t e);
static void cb_btn_xenium_dump(lv_obj_t *ta, lv_event_t e);
static void cb_btn_xenium_write_raw(lv_obj_t *ta, lv_event_t e);
static void cb_btn_xenium_write_update(lv_obj_t *ta, lv_event_t e);
static void cb_btn_xenium_rgb(lv_obj_t *ta, lv_event_t e);
static void cb_btn_poweroff(lv_obj_t *ta, lv_event_t e);
static void cb_btn_cancel(lv_obj_t *ta, lv_event_t e);

//Called every so often to perform lower priority background tasks
static void background_task(lv_task_t *task);

/* Xenium Functions */
static uint16_t xenium_queue[256] = {0xFFFF};
static uint8_t xenium_tasks_complete = 0;
static uint8_t xenium_tasks_number = 0;
static uint8_t verify_buffer[XENIUM_FLASH_SECTOR_SIZE];
static uint8_t working_buffer[XENIUM_FLASH_SIZE];

static void xenium_task_handler(lv_task_t *task);
static void xenium_queue_command(enum xenium_sector_actions action);
static uint8_t xenium_get_task_progress();

void create_menu(void)
{
    int screen_w = lv_obj_get_width(lv_scr_act());
    int screen_h = lv_obj_get_height(lv_scr_act());

    /* Register input device */
    sdl_indev = lv_indev_get_next(NULL);
    input_group = lv_group_create();
    if (sdl_indev != NULL)
        lv_indev_set_group(sdl_indev, input_group);

    /* Create Xenium-Tools Title */
    LV_IMG_DECLARE(xenium_tools_header);
    lv_obj_t *header = lv_img_create(lv_scr_act(), NULL);
    lv_img_set_src(header, &xenium_tools_header);
    lv_obj_align(header, lv_scr_act(), LV_ALIGN_IN_TOP_MID, 0, YMARGIN);

    /* Create a List on the Active Screen for Menu Items with callbacks*/
    main_list = lv_list_create(lv_scr_act(), NULL);
    lv_obj_set_height(main_list, screen_h - 200);
    lv_obj_set_width(main_list, screen_w - XMARGIN * 2);
    lv_obj_align(main_list, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, XMARGIN, YMARGIN + 65);

    lv_obj_set_event_cb(lv_list_add_btn(main_list, LV_SYMBOL_REFRESH, "Rescan for Xenium"), cb_btn_xenium_rescan);
    lv_obj_set_event_cb(lv_list_add_btn(main_list, LV_SYMBOL_DOWNLOAD, "Dump Xenium Flash Memory"), cb_btn_xenium_dump);
    lv_obj_set_event_cb(lv_list_add_btn(main_list, LV_SYMBOL_UPLOAD, "Write a Raw 2Mb Flash Dump"), cb_btn_xenium_write_raw);
    lv_obj_set_event_cb(lv_list_add_btn(main_list, LV_SYMBOL_UPLOAD, "Write XeniumOS 2.3.1 Update File"), cb_btn_xenium_write_update);
    lv_obj_set_event_cb(lv_list_add_btn(main_list, LV_SYMBOL_LOOP, "Cycle the Xenium RGB LED"), cb_btn_xenium_rgb);
    lv_obj_set_event_cb(lv_list_add_btn(main_list, LV_SYMBOL_TRASH, "Cancel Current Action"), cb_btn_cancel);
    lv_obj_set_event_cb(lv_list_add_btn(main_list, LV_SYMBOL_POWER, "Power Off"), cb_btn_poweroff);

    lv_group_add_obj(input_group, main_list); //Register it with the input driver

    /* Create a text label above progress bar for status */
    status_label = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_align(status_label, LV_LABEL_ALIGN_CENTER); /*Center aligned lines*/
    lv_label_set_text(status_label, "Idle");
    lv_obj_set_width(status_label, 150);
    lv_obj_align(status_label, NULL, LV_ALIGN_IN_BOTTOM_LEFT, XMARGIN, -YMARGIN - 30);
    static lv_style_t status_label_style;
    lv_style_init(&status_label_style);
    lv_style_set_text_font(&status_label_style, LV_STATE_DEFAULT, &lv_font_montserrat_18);
    lv_obj_add_style(status_label, LV_LABEL_PART_MAIN, &status_label_style);

    /* Create a Xenium Detection status */
    xenium_status = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_align(xenium_status, LV_LABEL_ALIGN_CENTER); /*Center aligned lines*/
    lv_label_set_text(xenium_status, "Xenium Status:");
    lv_obj_set_width(xenium_status, 150);
    lv_obj_align(xenium_status, NULL, LV_ALIGN_IN_BOTTOM_LEFT, XMARGIN, -YMARGIN - 30 - 30);
    lv_obj_align(xenium_status, NULL, LV_ALIGN_IN_BOTTOM_LEFT, XMARGIN, -YMARGIN - 30 - 30);
    lv_obj_add_style(xenium_status, LV_LABEL_PART_MAIN, &status_label_style);

    /* Create a orb to represent the Xenium Detection status */
    xenium_status_orb = lv_led_create(lv_scr_act(), NULL);
    lv_obj_align(xenium_status_orb, NULL, LV_ALIGN_CENTER, -80, 0);
    lv_style_init(&xenium_status_orb_style);
    lv_style_set_bg_color(&xenium_status_orb_style, LV_STATE_DEFAULT, LV_COLOR_RED);
    lv_style_set_border_color(&xenium_status_orb_style, LV_STATE_DEFAULT, LV_COLOR_RED);
    lv_style_set_shadow_color(&xenium_status_orb_style, LV_STATE_DEFAULT, LV_COLOR_RED);
    lv_obj_add_style(xenium_status_orb, LV_LABEL_PART_MAIN, &xenium_status_orb_style);
    lv_obj_set_size(xenium_status_orb, 10, 10);
    lv_obj_align(xenium_status_orb, NULL, LV_ALIGN_IN_BOTTOM_LEFT, XMARGIN + 150, -YMARGIN - 30 - 30);
    lv_led_on(xenium_status_orb);

    /* Create a progress bar */
    progress_bar = lv_bar_create(lv_scr_act(), NULL);
    lv_obj_set_size(progress_bar, screen_w - 2 * XMARGIN, 20);
    lv_obj_align(progress_bar, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -YMARGIN);
    lv_bar_set_anim_time(progress_bar, 200);
    lv_bar_set_value(progress_bar, 50, LV_ANIM_ON);

    /* Create an alert message box */
    alert_box = lv_textarea_create(lv_scr_act(), NULL);
    lv_textarea_set_text(alert_box, "");
    lv_textarea_set_cursor_hidden(alert_box, true);
    lv_obj_set_width(alert_box, screen_w * 0.80);
    lv_obj_set_height(alert_box, 100);
    lv_obj_align(alert_box, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_textarea_set_text_align(alert_box, LV_LABEL_ALIGN_CENTER);
    static lv_style_t alert_box_style;
    lv_style_init(&alert_box_style);
    lv_style_set_text_font(&alert_box_style, LV_STATE_DEFAULT, &lv_font_montserrat_20);
    lv_obj_add_style(alert_box, LV_LABEL_PART_MAIN, &alert_box_style);
    lv_obj_fade_out(alert_box, 0, 0);

    /* Setup the filesystem and file browser */
    lv_fs_dir_open(&cwd, fs_create_path(fs_id, dir_path));
    file_list = lv_list_create(lv_scr_act(), NULL);
    lv_obj_set_height(file_list, screen_h * 0.75);
    lv_obj_set_width(file_list, screen_w * 0.75);
    lv_obj_align(file_list, lv_scr_act(), LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_hidden(file_list, true);
    lv_group_add_obj(input_group, file_list);
    lv_obj_set_event_cb(file_list, file_list_cb);

    /* Setup the Xenium task handler and queue */
    xenium_tasks = lv_task_create(xenium_task_handler, 0, LV_TASK_PRIO_MID, NULL);
    memset(xenium_queue, 0xFF, sizeof(xenium_queue));

    /* Setup up slow background task to do things */
    background_tasks = lv_task_create(background_task, 100, LV_TASK_PRIO_MID, NULL);

    /* Force a Xenium rescan on first boot up */
    cb_btn_xenium_rescan(NULL, LV_EVENT_PRESSED);
}

/* Rescan the LPC header for a Xenium modchip */
static void cb_btn_xenium_rescan(lv_obj_t *ta, lv_event_t e)
{
    if (xenium_queue[0] != 0xFFFF)
        return;

    if (xenium_flash_busy())
        return;

    char buf[64];
    if (e == LV_EVENT_PRESSED)
    {
        const char *status;
        if (xenium_is_detected())
        {
            status = "Xenium Detected!";
            lv_led_on(xenium_status_orb);
        }
        else
        {
            status = "Xenium Not Detected";
            lv_led_off(xenium_status_orb);
        }
        lv_textarea_set_text(alert_box, status);
        lv_obj_fade_out(alert_box, 500, 700);
    }
    else if (e == LV_EVENT_CANCEL)
    {
        lv_textarea_set_text(alert_box, "Rescan the LPC header for a Xenium modchip\n");
        lv_obj_fade_out(alert_box, 500, 1500);
    }
}

/* Dump the flash memory from the Xenium */
static void cb_btn_xenium_dump(lv_obj_t *ta, lv_event_t e)
{
    if (xenium_queue[0] != 0xFFFF)
        return;

    if (lv_led_get_bright(xenium_status_orb) == LV_LED_BRIGHT_MIN)
        return;

    if (e == LV_EVENT_PRESSED)
    {
        for (int i = 0; i < XENIUM_FLASH_NUM_SECTORS; i++)
        {
            xenium_queue_command(READ_SECTOR + i);
        }
        xenium_queue_command(SAVE_RAWFLASH_TO_FILE);
        xenium_tasks_complete = 0;
        xenium_tasks_number = XENIUM_FLASH_NUM_SECTORS + 1;
    }
    else if (e == LV_EVENT_CANCEL)
    {
        lv_textarea_set_text(alert_box, "Dump the full Xenium flash chip to 'xenium_flash.bin.'");
        lv_obj_fade_out(alert_box, 500, 1500);
    }
}

/* Write a raw 2mb flash dump to the Xenium */
static void cb_btn_xenium_write_raw(lv_obj_t *ta, lv_event_t e)
{
    if (xenium_queue[0] != 0xFFFF)
        return;

    if (lv_led_get_bright(xenium_status_orb) == LV_LED_BRIGHT_MIN)
        return;

    if (e == LV_EVENT_PRESSED)
    {
        xenium_queue_command(SELECT_FILE);
        xenium_queue_command(READ_RAWFLASH_FROM_FILE);
        for (int i = 0; i < XENIUM_FLASH_NUM_SECTORS; i++)
        {
            xenium_queue_command(WRITE_SECTOR + i);
        }
        xenium_tasks_complete = 0;
        xenium_tasks_number = XENIUM_FLASH_NUM_SECTORS + 2;
    }
    else if (e == LV_EVENT_CANCEL)
    {
        lv_textarea_set_text(alert_box, "Write a raw 2mb flash dump to the Xenium");
        lv_obj_fade_out(alert_box, 500, 1500);
    }
}

/* Write a XOS 2.3.1 update file */
static void cb_btn_xenium_write_update(lv_obj_t *ta, lv_event_t e)
{
    if (xenium_queue[0] != 0xFFFF)
        return;

    if (e == LV_EVENT_PRESSED)
    {
        xenium_tasks_number = 0;
        xenium_queue_command(SELECT_FILE);
        xenium_queue_command(READ_RECOVERY_FLASH_FROM_FILE);
        xenium_tasks_number += 2;
        //XeniumOS and bootloader are here //0x100000 to 0x1C0000
        for (int i = 16; i <= 27; i++)
        {
            xenium_queue_command(WRITE_SECTOR + i);
            xenium_tasks_number++;
        }
        //XeniumOS additional data is here //1E0000 to 1FBFFF
        for (int i = 30; i <= 33; i++)
        {
            xenium_queue_command(WRITE_SECTOR + i);
            xenium_tasks_number++;
        }

        xenium_tasks_complete = 0;
    }
    else if (e == LV_EVENT_CANCEL)
    {
        lv_textarea_set_text(alert_box, "Write a XeniumOS v2.3.1 update file\n");
        lv_textarea_add_text(alert_box, "These can be found on the internet easily");
        lv_obj_fade_out(alert_box, 500, 1500);
    }
}

/* Cycle the Xenium's RGB LED */
static void cb_btn_xenium_rgb(lv_obj_t *ta, lv_event_t e)
{
    char buf[64];
    const char *led_str;
    if (e == LV_EVENT_PRESSED)
    {
        static int i = 0;
        lv_color_t c;
        xenium_set_led(i = (i + 1) % 8);
        switch (i)
        {
        case 0:
            led_str = "(Off)";
            c = LV_COLOR_BLACK;
            break;
        case 1:
            led_str = "(Red)";
            c = LV_COLOR_RED;
            break;
        case 2:
            led_str = "(Green)";
            c = LV_COLOR_GREEN;
            break;
        case 3:
            led_str = "(Amber)";
            c = LV_COLOR_ORANGE;
            break;
        case 4:
            led_str = "(Blue)";
            c = LV_COLOR_BLUE;
            break;
        case 5:
            led_str = "(Purple)";
            c = LV_COLOR_PURPLE;
            break;
        case 6:
            led_str = "(Teal)";
            c = LV_COLOR_TEAL;
            break;
        case 7:
            led_str = "(White)";
            c = LV_COLOR_WHITE;
            break;
        }

        lv_style_set_bg_color(&xenium_status_orb_style, LV_STATE_DEFAULT, c);
        lv_style_set_border_color(&xenium_status_orb_style, LV_STATE_DEFAULT, c);
        lv_style_set_shadow_color(&xenium_status_orb_style, LV_STATE_DEFAULT, c);
        lv_obj_add_style(xenium_status_orb, LV_LABEL_PART_MAIN, &xenium_status_orb_style);
        sprintf(buf, "Cycle the Xenium RGB LED %s", led_str);
        lv_label_set_text(lv_list_get_btn_label(ta), buf);
    }
    else if (e == LV_EVENT_CANCEL)
    {
        lv_textarea_set_text(alert_box, "Cycle the Xenium's RGB LED\n");
        lv_obj_fade_out(alert_box, 500, 1500);
    }
}

/* Power off the system */
static void cb_btn_poweroff(lv_obj_t *ta, lv_event_t e)
{
    if (xenium_flash_busy())
        return;

    if (xenium_queue[0] != 0xFFFF)
        return;

    if (e == LV_EVENT_PRESSED)
    {
        lv_label_set_text(status_label, "Powering off...");
        set_quit_event(true);
    }
    else if (e == LV_EVENT_CANCEL)
    {
        lv_textarea_set_text(alert_box, "Power of the system\n");
        lv_obj_fade_out(alert_box, 500, 1500);
    }
}

/* Cancel current queue */
static void cb_btn_cancel(lv_obj_t *ta, lv_event_t e)
{
    if (e == LV_EVENT_PRESSED)
    {
        memset(xenium_queue, 0xFF, sizeof(xenium_queue));
        lv_label_set_text(status_label, "Idle");
        xenium_tasks_number = 0;
    }
    else if (e == LV_EVENT_CANCEL)
    {
        lv_textarea_set_text(alert_box, "Cancel the current task\n");
        lv_textarea_add_text(alert_box, "WARNING: Interrupting a flash write can corrupt the Xenium\n");
        lv_obj_fade_out(alert_box, 500, 1500);
    }
}

/* lvgl fs driver expects a drive letter prefixed to every filename/path
 * so that it can find the registerd fs driver. This adds that */
static char *fs_create_path(const char *fs_id, const char *path)
{
    static char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s%s", fs_id, path);
    return full_path;
}

/* Call back when an event occurs on the filebrowser */
static void file_list_cb(lv_obj_t *ta, lv_event_t e)
{
    if (e == LV_EVENT_PRESSED)
    {
        lv_obj_t *btn = lv_list_get_btn_selected(ta);
        strncpy(selected_file_name, lv_list_get_btn_text(btn), sizeof(selected_file_name));
        lv_obj_set_hidden(file_list, true);
        lv_group_focus_obj(main_list);
    }
    else if (e == LV_EVENT_CANCEL)
    {
        strncpy(selected_file_name, "", sizeof(selected_file_name));
        lv_obj_set_hidden(file_list, true);
        lv_group_focus_obj(main_list);
        lv_btn_set_state(lv_list_get_btn_selected(main_list), LV_BTN_STATE_RELEASED);
    }
}

/* Any low priority background tasks can happen here */
static void background_task(lv_task_t *task)
{
    lv_bar_set_value(progress_bar, xenium_get_task_progress(), LV_ANIM_ON);
}

/* Queue a command to be perform on the Xenium asynchronously */
static void xenium_queue_command(enum xenium_sector_actions action)
{
    int i = 0;
    while (xenium_queue[i] != 0xFFFF && i < (sizeof(xenium_queue) / sizeof(xenium_queue[0])))
    {
        i++;
    }
    xenium_queue[i] = action;
}

/* Returns the progress of the current task queue. 0-100 */
static uint8_t xenium_get_task_progress()
{
    if (xenium_tasks_number == 0)
        return 100;

    return xenium_tasks_complete * 100 / xenium_tasks_number;
}

//Sort an array alphabetically
static int compare(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}
static void sort(char **string_array, int len)
{
    qsort(string_array, len, sizeof(char *), compare);
}

/* Main Xenium statemachine which executes items from the task queue */
static void xenium_task_handler(lv_task_t *task)
{
    if (xenium_queue[0] == 0xFFFF)
        return;

    static uint8_t sector;
    static enum xenium_state state = FINISHED;
    static uint8_t old_bank = 1;
    static uint8_t sector_erased = 0;
    static uint32_t byte_cnt = 0;
    uint32_t add, len;

    //What type of command is queued
    switch (xenium_queue[0] & 0xF000)
    {
    case WRITE_SECTOR:
        state = FLASH_WRITING;
        break;
    case READ_SECTOR:
        state = FLASH_READING;
        break;
    case SAVE_RAWFLASH_TO_FILE:
    case READ_RAWFLASH_FROM_FILE:
    case READ_RECOVERY_FLASH_FROM_FILE:
    case SELECT_FILE:
        state = FILE_IO;
        break;
    }

    //If its a flash access, extract the sector number from the command
    if (state != FILE_IO)
    {
        sector = xenium_queue[0] & ~0xF000;
        add = xenium_sector_to_address(sector);
        len = xenium_sector_to_size(sector);
    }

    switch (state)
    {
    case FLASH_READING:
        lv_label_set_text(status_label, "Reading Flash");
        old_bank = xenium_get_bank();
        xenium_set_bank(xenium_sector_to_bank(sector));
        xenium_flash_read_stream(add, &working_buffer[add], len);
        xenium_set_bank(old_bank);
        state = FINISHED;
        break;

    case FLASH_WRITING:
        lv_label_set_text(status_label, "Writing Flash. Do Not Power Off!");
        if (xenium_flash_busy())
            break;

        if (sector_erased == 0)
        {
            printf("Erasing sector %u\n", sector);
            old_bank = xenium_get_bank();
            xenium_set_bank(xenium_sector_to_bank(sector));
            xenium_start_sector_erase(add);
            sector_erased = 1;
            break;
        }

        if (byte_cnt < len)
        {
            if (byte_cnt == 0)
                printf("Writing sector %u\n", sector);
            xenium_start_flash_program_byte(add + byte_cnt, working_buffer[add + byte_cnt]);
            byte_cnt++;
            break;
        }
        else
        {
            sector_erased = 0;
            byte_cnt = 0;
            printf("Verify sector %u ", sector);
            memset(verify_buffer, 0xFF, sizeof(verify_buffer));
            xenium_flash_read_stream(add, verify_buffer, len);
            //Very data, if error, reset chip and try again. Otherwise sector is done!
            if (memcmp(&working_buffer[add], verify_buffer, len) != 0)
            {
                printf("ERROR at sector %u!\n", sector);
                lv_textarea_set_text(alert_box, "Error writing to Xenium.");
                lv_obj_fade_out(alert_box, 500, 1500);
            }
            else
            {
                printf("OK!\n");
                state = FINISHED;
            }
        }
        break;
    case FILE_IO:
        if (xenium_queue[0] == SAVE_RAWFLASH_TO_FILE)
        {
            lv_fs_dir_open(&cwd, fs_create_path(fs_id, ""));
            lv_fs_file_t file_p;
            unsigned int bytes_written = 0;
            const char *file_name = "xenium_flash.bin";
            bool error = 1;
            do
            {
                //Create a new file, (overwrite if already exists)
                if (lv_fs_open(&file_p, fs_create_path(fs_id, file_name), LV_FS_MODE_WR) != LV_FS_RES_OK)
                    break;

                if (lv_fs_write(&file_p, working_buffer, sizeof(working_buffer), &bytes_written) != LV_FS_RES_OK)
                    break;

                //Close the file
                lv_fs_close(&file_p);

                error = 0;
            } while (0);

            if (error)
            {
                lv_textarea_set_text(alert_box, "Could not save ");
                lv_textarea_add_text(alert_box, file_name);
            }
            else
            {
                lv_textarea_set_text(alert_box, "Saved ");
                lv_textarea_add_text(alert_box, file_name);
                lv_textarea_add_text(alert_box, " OK!");
            }
            lv_fs_dir_close(&cwd);
            lv_obj_fade_out(alert_box, 500, 1500);
            state = FINISHED;
        }
        else if (xenium_queue[0] == READ_RAWFLASH_FROM_FILE || xenium_queue[0] == READ_RECOVERY_FLASH_FROM_FILE)
        {
            if (!lv_obj_is_focused(file_list) && selected_file_name[0] == '\0')
            {
                //Cancelled by user
                state = FINISHED_WITH_ERROR;
            }

            //A file has been selected
            else if (strlen(selected_file_name) > 0)
            {
                lv_fs_dir_open(&cwd, fs_create_path(fs_id, ""));
                lv_fs_file_t file_p;
                unsigned int file_size = 0;
                unsigned int bytes_read = 0;
                bool error = 1;
                do
                {
                    //Create a new file, (overwrite if already exists)
                    if (lv_fs_open(&file_p, fs_create_path(fs_id, selected_file_name), LV_FS_MODE_RD) != LV_FS_RES_OK)
                    {
                        lv_textarea_set_text(alert_box, "Could not open file for reading");
                        break;
                    }

                    if (lv_fs_size(&file_p, &file_size) != LV_FS_RES_OK)
                    {
                        lv_textarea_set_text(alert_box, "Could not determine file size");
                        break;
                    }

                    if (file_size != XENIUM_FLASH_SIZE && xenium_queue[0] == READ_RAWFLASH_FROM_FILE)
                    {
                        lv_textarea_set_text(alert_box, "File size does not match flash size");
                        break;
                    }

                    if (file_size != 1050964 && xenium_queue[0] == READ_RECOVERY_FLASH_FROM_FILE)
                    {
                        lv_textarea_set_text(alert_box, "File size does not match supported recovery size");
                        break;
                    }

                    if (xenium_queue[0] == READ_RAWFLASH_FROM_FILE)
                    {
                        if (lv_fs_read(&file_p, working_buffer, sizeof(working_buffer), &bytes_read) != LV_FS_RES_OK)
                        {
                            lv_textarea_set_text(alert_box, "Could not read file");
                            break;
                        }
                    }
                    //For the recovery.bin update file, only read from specified offsets
                    else if (xenium_queue[0] == READ_RECOVERY_FLASH_FROM_FILE)
                    {
                        if (lv_fs_seek(&file_p, 0x00948) != LV_FS_RES_OK)
                            break;
                        if (lv_fs_read(&file_p, &working_buffer[0x180000], 0x40000, &bytes_read) != LV_FS_RES_OK)
                            break;
                        if (lv_fs_seek(&file_p, 0x40948) != LV_FS_RES_OK)
                            break;
                        if (lv_fs_read(&file_p, &working_buffer[0x100000], 0x80000, &bytes_read) != LV_FS_RES_OK)
                            break;
                        if (lv_fs_seek(&file_p, 0xE0948) != LV_FS_RES_OK)
                            break;
                        if (lv_fs_read(&file_p, &working_buffer[0x1E0000], 0x1BFFF, &bytes_read) != LV_FS_RES_OK)
                            break;
                    }

                    //Close the file
                    lv_fs_close(&file_p);
                    lv_fs_dir_close(&cwd);

                    error = 0;
                } while (0);

                if (error == 0)
                {
                    lv_textarea_set_text(alert_box, "Read ");
                    lv_textarea_add_text(alert_box, selected_file_name);
                    lv_textarea_add_text(alert_box, " OK!");
                    state = FINISHED;
                }
                else
                {
                    lv_textarea_add_text(alert_box, "\nERROR");
                    state = FINISHED_WITH_ERROR;
                }
                lv_obj_fade_out(alert_box, 500, 1500);
            }
        }
        else if (xenium_queue[0] == SELECT_FILE)
        {
            //Show the file list window and take focus
            lv_list_clean(file_list);
            lv_obj_set_hidden(file_list, false);
            lv_group_focus_obj(file_list);

            //Clear old file name
            memset(selected_file_name, 0x00, sizeof(selected_file_name));

            //Scan directory for files
            lv_fs_dir_open(&cwd, fs_create_path(fs_id, ""));

            int item_cnt = 0;
            char **item_array = malloc(256 * sizeof(char *));
            char fn[256];
            while (lv_fs_dir_read(&cwd, fn) == LV_FS_RES_OK)
            {
                item_array[item_cnt] = (char *)malloc(strlen(fn) + 1);
                strncpy(item_array[item_cnt], fn, strlen(fn) + 1);
                item_cnt++;
            }
            sort(item_array, item_cnt);
            for (int i = 0; i < item_cnt; i++)
            {
                lv_list_add_btn(file_list, (item_array[i][0] == '/') ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE, item_array[i]);
                free(item_array[i]);
            }
            free(item_array);
            lv_fs_dir_close(&cwd);
            state = FINISHED;
        }
        break;
    case FINISHED:
    case FINISHED_WITH_ERROR:
        break;
    }

    if (state == FINISHED)
    {
        xenium_tasks_complete++;
        int array_len = sizeof(xenium_queue) / sizeof(xenium_queue[0]);
        for (int i = 0; i < array_len - 1; i++)
        {
            xenium_queue[i] = xenium_queue[i + 1];
        }
        xenium_queue[array_len - 1] = 0xFFFF;

        if (xenium_queue[0] == 0xFFFF)
            lv_label_set_text(status_label, "Idle");
    }
    else if (state == FINISHED_WITH_ERROR)
    {
        memset(xenium_queue, 0xFF, sizeof(xenium_queue));
        lv_label_set_text(status_label, "Idle");
        xenium_tasks_number = 0;
    }
}