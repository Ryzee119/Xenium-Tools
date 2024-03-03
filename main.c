#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_OPENGL2
#define CIMGUI_USE_SDL2
#define STB_IMAGE_IMPLEMENTATION
//      STB_SPRINTF_IMPLEMENTATION (Done in imgui)
#define GL_GLEXT_PROTOTYPES
#define MARGIN 50
#define ROOT_DIR "Q:" // Q is mounted as the path of this program's xbe
#define XT_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#include "main.h"
#include "xenium-header.h"

#define COLOUR_GREEN IM_COL32(0, 128, 0, 255)
#define COLOUR_ORANGE IM_COL32(128, 128, 0, 255)
#define COLOUR_RED IM_COL32(128, 0, 0, 255)
#define COLOUR_BLACK IM_COL32(0, 0, 0, 255)

static bool quit = false, reboot = false, write_busy = false, write_start_raw = false, write_start_update = false;
static char info_text[256];
static const char *default_info_text = "By Ryzee119";
static char **file_list = NULL;
static unsigned int file_count = 0;
static unsigned int bytes_remaining = 0;
static unsigned int bytes_total = 0;
static unsigned int validation_errors = 0;
static uint8_t file_data[XENIUM_FLASH_SIZE];
static uint32_t text_colour = COLOUR_BLACK;
static uint32_t led_colour = COLOUR_BLACK;

void usbh_core_deinit(void);
bool create_label(const char *window_name, label_align_t alignment, ImVec2 offset, ImGuiWindowFlags flags,
                  const char *format, ...);
int write_flash_update(void *data);
int write_flash_raw(void *data);
char **list_files_in_folder(const char *folder_path, unsigned int *file_count);
bool open_file_browser(char *selected_file, const char **file_list, const int file_count);
void font_override_default();

// We want imgui too allocate vertex buffers to contiguous memory; so we use a custom allocater
#define IMGUI_POOL_SIZE (PAGE_SIZE * 2048)
static void _ImGuiMemInit(tlsf_t *pool)
{
    void *data = pbgl_alloc(IMGUI_POOL_SIZE, 1);
    *pool = tlsf_create_with_pool(data, IMGUI_POOL_SIZE);
}
static void *_ImGuiMemAllocFunc(size_t sz, void *user_data)
{
    void *p = tlsf_malloc(user_data, sz);
    return p;
}
static void _ImGuiMemFreeFunc(void *ptr, void *user_data)
{
    tlsf_free(user_data, ptr);
}

void xbox_get_memory_usage(uint32_t *used, uint32_t *total)
{
    ULONG mem_size, mem_used;
    MM_STATISTICS MemoryStatistics;
    MemoryStatistics.Length = sizeof(MM_STATISTICS);
    MmQueryStatistics(&MemoryStatistics);
    if (total)
    {
        *total = (MemoryStatistics.TotalPhysicalPages * PAGE_SIZE);
    }
    if (used)
    {
        *used = (MemoryStatistics.TotalPhysicalPages - MemoryStatistics.AvailablePages) * PAGE_SIZE;
    }
}

int main()
{
    // Mount xbe root dir to Q:
    char launcher_path[MAX_PATH];
    nxGetCurrentXbeNtPath(launcher_path);
    *(strrchr(launcher_path, '\\') + 1) = '\0';
    nxMountDrive('Q', launcher_path);

    // Setup video output and GPU state
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
    pbgl_init(GL_TRUE);
    pbgl_set_swap_interval(2); // 30fps

    // Use our own memory allocator for ImGui
    tlsf_t memory_pool;
    _ImGuiMemInit(&memory_pool);
    igSetAllocatorFunctions(_ImGuiMemAllocFunc, _ImGuiMemFreeFunc, memory_pool);

    // Enable USB and input
    SDL_Init(SDL_INIT_GAMECONTROLLER);

    // Initialize imgui
    igCreateContext(NULL);
    ImGui_ImplSDL2_InitForOpenGL(NULL, NULL);
    ImGui_ImplOpenGL2_Init();

    ImGuiIO *ioptr = igGetIO();
    ioptr->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ioptr->DisplaySize.x = 640 - 1;
    ioptr->DisplaySize.y = 480 - 1;
    ioptr->FontGlobalScale = 2.0f;

    // You need to do this to disable AA on fonts
    ImGuiStyle *style = igGetStyle();
    style->AntiAliasedLinesUseTex = false;
    ioptr->Fonts->Flags |= ImFontAtlasFlags_NoBakedLines;

    // Imgui debug output
    // ImGuiContext *ctx = igGetCurrentContext();
    // ctx->DebugLogFlags = ImGuiDebugLogFlags_EventMask_ | ImGuiDebugLogFlags_OutputToTTY;

    igStyleColorsDark(NULL);
    ImFontAtlas_AddFontDefault(ioptr->Fonts, NULL);

    // Bit hacky, but override the backend font object and load it in manually with GL_NEAREST filtering
    GLuint FontTexture;
    GLuint *bd = (GLuint *)ioptr->BackendRendererUserData;
    unsigned char *pixels;
    int width, height;
    ImFontAtlas_GetTexDataAsRGBA32(ioptr->Fonts, &pixels, &width, &height, NULL);
    glGenTextures(1, &FontTexture);
    glBindTexture(GL_TEXTURE_2D, FontTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    ImFontAtlas_SetTexID(ioptr->Fonts, (ImTextureID)FontTexture);
    *bd = FontTexture;

    // Load in the header image
    int header_x, header_y;
    void *image_data =
        stbi_load_from_memory(XENIUM_HEADER_PNG, sizeof(XENIUM_HEADER_PNG), &header_x, &header_y, NULL, 4);
    ImTextureID xenium_header_id;
    glGenTextures(1, (GLuint *)&xenium_header_id);
    glBindTexture(GL_TEXTURE_2D, (GLuint)xenium_header_id);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, header_x, header_y, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);

    // Prep default information text
    strcpy(info_text, default_info_text);

    while (!quit)
    {
        /* Handle input */
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0)
        {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT)
                quit = true;
        }

        /* Prep for a new frame */
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        igNewFrame();

        /* Draw */
        // Background
        igSetNextWindowPos((ImVec2){0, 0}, ImGuiCond_None, (ImVec2){0, 0});
        igSetNextWindowSize((ImVec2){ioptr->DisplaySize.x, ioptr->DisplaySize.y}, ImGuiCond_None);
        if (igBegin("background", NULL,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs))
        {
            ImDrawList_AddRectFilledMultiColor(igGetWindowDrawList(), (ImVec2){0, 0},
                                               (ImVec2){ioptr->DisplaySize.x, ioptr->DisplaySize.y},
                                               IM_COL32(162, 213, 205, 255), IM_COL32(162, 213, 205, 255),
                                               IM_COL32(49, 91, 88, 255), IM_COL32(49, 91, 88, 255));
        }
        igEnd();

        // Header
        igPushStyleVar_Vec2(ImGuiStyleVar_FramePadding, (ImVec2){0.0f, 0.0f});
        igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, (ImVec2){0.0f, 0.0f});
        igSetNextWindowPos((ImVec2){(ioptr->DisplaySize.x - header_x) / 2, MARGIN}, ImGuiCond_None, (ImVec2){0, 0});
        igSetNextWindowSize((ImVec2){header_x, header_y}, ImGuiCond_None);
        if (igBegin("header", NULL,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs |
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground))
        {
            igPushStyleVar_Float(ImGuiStyleVar_ChildBorderSize, 0.0f);
            igImage(xenium_header_id, (ImVec2){header_x, header_y}, (ImVec2){0, 0}, (ImVec2){1, 1},
                    (ImVec4){0, 0, 0, 1}, (ImVec4){0, 0, 0, 0});
            igPopStyleVar(1);
        }
        igEnd();
        igPopStyleVar(2);

        // Info Text
        igPushStyleColor_U32(ImGuiCol_Text, text_colour);
        create_label("info", ALIGN_TOP_CENTER, (ImVec2){0, header_y},
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs, info_text);
        igPopStyleColor(1);

        // Main List
        igSetNextWindowPos((ImVec2){MARGIN, 125}, ImGuiCond_None, (ImVec2){0, 0});
        igSetNextWindowSize((ImVec2){ioptr->DisplaySize.x - MARGIN * 2, ioptr->DisplaySize.y - 175}, ImGuiCond_None);
        if (igBegin("menu", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
        {
            static const char *list_items[] = {"Check Flash Chip",
                                               "Dump Xenium Flash Memory",
                                               "Write a Raw 2MB Flash Dump",
                                               "Write XeniumOS 2.3.1 File",
                                               "Cycle the Xenium RGB LED",
                                               "Exit",
                                               "Power Off"};
            ImVec2 win_size;
            igGetContentRegionAvail(&win_size);

            igPushStyleVar_Vec2(ImGuiStyleVar_ButtonTextAlign, (ImVec2){0.0f, 0.5f});
            for (int i = 0; i < 7; i++)
            {
                if (igButton(list_items[i], (ImVec2){win_size.x, win_size.y / XT_ARRAY_SIZE(list_items) -
                                                                     igGetStyle()->FramePadding.y}))
                {
                    switch (i)
                    {
                    case 0: // Check Flash Chip
                        if (write_busy)
                            break;
                        uint8_t manufacturer_id, device_id;
                        xenium_chip_ids(&manufacturer_id, &device_id);
                        if (device_id == 0xC4)
                        {
                            text_colour = COLOUR_GREEN;
                        }
                        else
                        {
                            text_colour = COLOUR_ORANGE;
                        }
                        stbsp_snprintf(info_text, sizeof(info_text), "Man ID: 0x%02X Dev ID: 0x%02x",
                                       manufacturer_id, device_id);
                        break;
                    case 1: // Dump Xenium Flash Memory
                        if (write_busy)
                            break;
                        xenium_flash_read_stream(0, file_data, XENIUM_FLASH_SIZE);

                        SYSTEMTIME st;
                        GetLocalTime(&st);
                        char path[MAX_PATH];
                        // path is like flash_YYYY-MM-DD HHMMSS.bin
                        stbsp_snprintf(path, sizeof(path), "%s\\flash_%04d%02d%02d %02d%02d%02d.bin", ROOT_DIR,
                                       st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
                        FILE *fp = fopen(path, "wb");
                        if (fp)
                        {
                            fwrite(file_data, XENIUM_FLASH_SIZE, 1, fp);
                            fclose(fp);
                            text_colour = COLOUR_GREEN;
                            stbsp_snprintf(info_text, sizeof(info_text), "Wrote to %s", path);
                        }
                        else
                        {
                            text_colour = COLOUR_RED;
                            stbsp_snprintf(info_text, sizeof(info_text), "Failed to write to flash.bin");
                        }
                        break;
                    case 2: // Write a Raw 2MB Flash Dump
                    case 3: // Write XeniumOS 2.3.1 File
                        if (write_busy)
                        {
                            break;
                        }
                        else
                        {
                            file_list = list_files_in_folder(ROOT_DIR, &file_count);
                            write_start_raw = (i == 2);
                            write_start_update = (i == 3);
                        }
                        break;
                    case 4: // Cycle the Xenium RGB LED
                        static int led_index = 0;
                        static const char *led_colors[] = {"Off", "Red", "Green", "Amber",
                                                           "Blue", "Purple", "Teal", "White"};
                        xenium_set_led(led_index);
                        // Pull bits out of led index to work out led colour
                        uint8_t r = ((led_index >> 0) & 0b1) * 128;
                        uint8_t g = ((led_index >> 1) & 0b1) * 128;
                        uint8_t b = ((led_index >> 2) & 0b1) * 128;
                        text_colour = IM_COL32(r, g, b, 255);
                        stbsp_snprintf(info_text, sizeof(info_text), "Led set to %s", led_colors[led_index]);
                        led_index = (led_index + 1) % XT_ARRAY_SIZE(led_colors);
                        break;
                    case 5: // Exit
                        quit = true;
                        reboot = true;
                        break;
                    case 6: // Power Off
                        quit = true;
                        reboot = false;
                        break;
                    default:
                        break;
                    }
                }
            }
            igPopStyleVar(1);

            // Progress bar
            igProgressBar((bytes_total) ? 1.0f - ((float)bytes_remaining / (float)bytes_total) : 1.0f,
                          (ImVec2){win_size.x, 3}, NULL);

            // Handle file browser
            if (write_start_raw || write_start_update)
            {
                char selected_file[MAX_PATH];
                // Open the file browser and wait for it to be closed by the user (either file selected for just
                // cancelled)
                if (open_file_browser(selected_file, (const char **)file_list, file_count) == false)
                {
                    // No longer need the file list. Clean up
                    for (int i = 0; i < file_count; i++)
                    {
                        free(file_list[i]);
                    }
                    free(file_list);
                    file_list = NULL;

                    // Did the user select a file?
                    // Read it in, check it etc.
                    if (selected_file[0] != '\0')
                    {
                        size_t wanted_file_size = (write_start_raw) ? XENIUM_FLASH_SIZE : XENIUM_FLASH_UPDATE_FILE_SIZE;
                        size_t file_size = 0;
                        size_t rb = 0;
                        FILE *fp = fopen(selected_file, "rb");
                        if (fp)
                        {
                            fseek(fp, 0, SEEK_END);
                            file_size = ftell(fp);
                            fseek(fp, 0, SEEK_SET);
                            if (file_size == wanted_file_size)
                            {
                                rb = fread(file_data, 1, file_size, fp);
                                fclose(fp);
                            }
                        }
                        if (rb == wanted_file_size)
                        {
                            bytes_remaining = wanted_file_size;
                            bytes_total = wanted_file_size;
                            write_busy = true;
                            validation_errors = 0;
                            // Flash writes occur in a separate thread to keep gui responsive
                            if (write_start_raw)
                            {
                                SDL_CreateThread(write_flash_raw, "write_flash_raw", file_data);
                            }
                            else
                            {
                                SDL_CreateThread(write_flash_update, "write_flash_update", file_data);
                            }
                        }
                        else
                        {
                            text_colour = COLOUR_RED;
                            if (rb == 0)
                            {
                                stbsp_snprintf(info_text, sizeof(info_text), "File size mismatch. %dkB/%dkB\n",
                                               file_size / 1024, wanted_file_size / 1024);
                            }
                            else
                            {
                                stbsp_snprintf(info_text, sizeof(info_text), "Failed to open %s", selected_file);
                            }
                        }
                    }

                    write_start_raw = false;
                    write_start_update = false;
                }
            }

            if (write_busy)
            {
                if (bytes_remaining == 0)
                {
                    if (validation_errors == 0)
                    {
                        text_colour = COLOUR_GREEN;
                    }
                    else
                    {
                        text_colour = COLOUR_ORANGE;
                    }
                    stbsp_snprintf(info_text, sizeof(info_text), "Write complete. Errors %d", validation_errors);
                    write_busy = false;
                }
                else
                {
                    text_colour = COLOUR_BLACK;
                    stbsp_snprintf(info_text, sizeof(info_text), "Writing %dkB/%dkB, Errors: %d",
                                   (bytes_total - bytes_remaining) / 1024, bytes_total / 1024, validation_errors);
                }
            }

#if (1)
            uint32_t system_used, system_total;
            xbox_get_memory_usage(&system_used, &system_total);
            create_label("fpsdisplay", ALIGN_BOTTOM_RIGHT, (ImVec2){MARGIN - 1, MARGIN - 1},
                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing,
                         "FPS: %.1f, System %05d/%05d\n", igGetIO()->Framerate, system_used / 1024,
                         system_total / 1024);
#endif
        }
        igEnd();

        igRender();
        glViewport(0, 0, (int)ioptr->DisplaySize.x, (int)ioptr->DisplaySize.y);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        ImGui_ImplOpenGL2_RenderDrawData(igGetDrawData());
        pbgl_swap_buffers();
    }

    pbgl_shutdown();
    SDL_Quit();
    usbh_core_deinit();

    if (reboot)
    {
        HalReturnToFirmware(HalRebootRoutine);
    }
    else
    {
        HalInitiateShutdown();
    }
    return 0;
}

int write_flash_raw(void *file_data)
{
    xenium_flash_program_stream(0, file_data, &bytes_remaining, &validation_errors);
    bytes_remaining = 0;
    return 0;
}

int write_flash_update(void *selected_file_path)
{
    uint32_t chunk_len;

    uint8_t *_flash_data = malloc(XENIUM_FLASH_SIZE);
    memset(_flash_data, 0xFF, XENIUM_FLASH_SIZE);
    // BOOTLOADERLOADER - Exists at offset 0x00948 in recovery.bin
    memcpy(&_flash_data[0x180000], &file_data[0x00948], 0x40000);
    // XENIUMOS PART1 - Exists at offset 0x40948 in recovery.bin
    memcpy(&_flash_data[0x100000], &file_data[0x40948], 0x80000);
    // XENIUMOS PART2 - Exists at offset 0xE0948 in recovery.bin
    memcpy(&_flash_data[0x1E0000], &file_data[0xE0948], 0x20000);

    bytes_remaining = XENIUM_FLASH_SIZE;
    bytes_total = XENIUM_FLASH_SIZE;

    xenium_flash_program_stream(0, _flash_data, &bytes_remaining, &validation_errors);
    bytes_remaining = 0;
    free(_flash_data);

    return 0;
}

int dump_flash_contents(void *arg)
{
    return 0;
}

bool create_label(const char *window_name, label_align_t alignment, ImVec2 offset, ImGuiWindowFlags flags,
                  const char *format, ...)
{
    char text[256];
    ImVec2 text_size;
    ImGuiIO *ioptr;
    va_list args;
    bool opened;

    va_start(args, format);
    stbsp_vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    ioptr = igGetIO();
    igCalcTextSize(&text_size, text, NULL, false, 0.0f);
    text_size.x += igGetStyle()->WindowPadding.x * 2;
    text_size.y += igGetStyle()->WindowPadding.y * 2;

    ImVec2 pos;
    float right_edge = ioptr->DisplaySize.x - text_size.x - MARGIN;
    float bottom_edge = ioptr->DisplaySize.y - text_size.y - MARGIN;
    float center_y = ioptr->DisplaySize.y / 2 - text_size.y / 2;
    float center_x = ioptr->DisplaySize.x / 2 - text_size.x / 2;
    float x = MARGIN, y = MARGIN;
    ;

    switch (alignment)
    {
    case ALIGN_TOP_LEFT:
        break;
    case ALIGN_TOP_RIGHT:
        x = right_edge;
        break;
    case ALIGN_BOTTOM_LEFT:
        y = bottom_edge;
        break;
    case ALIGN_BOTTOM_RIGHT:
        x = right_edge;
        y = bottom_edge;
        break;
    case ALIGN_CENTER_CENTER:
        x = center_x;
        y = center_y;
        break;
    case ALIGN_CENTER_LEFT:
        y = center_y;
        break;
    case ALIGN_CENTER_RIGHT:
        x = right_edge;
        y = center_y;
        break;
    case ALIGN_TOP_CENTER:
        x = center_x;
        break;
    case ALIGN_BOTTOM_CENTER:
        x = center_x;
        y = bottom_edge;
        break;
    default:
        break;
    }

    x += offset.x;
    y += offset.y;

    pos = (ImVec2){x, y};
    opened = true;

    igSetNextWindowPos(pos, ImGuiCond_None, (ImVec2){0, 0});
    igSetNextWindowSize(text_size, ImGuiCond_Always);
    if (igBegin(window_name, NULL, ImGuiWindowFlags_NoTitleBar | flags))
    {
        igText(text);
    }
    igEnd();
    return opened;
}

static int file_sorter(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

char **list_files_in_folder(const char *folder_path, unsigned int *file_count)
{
    WIN32_FIND_DATA findFileData;
    HANDLE hFind;
    char searchPath[MAX_PATH];

    sprintf(searchPath, "%s\\*", folder_path);

    hFind = FindFirstFile(searchPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        *file_count = 0;
        return NULL;
    }

    int index = 0;
    int max_files = 256; // Initial capacity
    char **files = (char **)malloc(max_files * sizeof(char *));
    if (files == NULL)
    {
        goto error_out;
    }

    do
    {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            files[index] = (char *)malloc((strlen(findFileData.cFileName) + 1) * sizeof(char));
            if (files[index] == NULL)
            {
                goto error_out;
            }
            strcpy(files[index], findFileData.cFileName);
            index++;

            // Resize the array if needed
            if (index >= max_files)
            {
                max_files *= 2;
                files = (char **)realloc(files, max_files * sizeof(char *));
                if (files == NULL)
                {
                    goto error_out;
                }
            }
        }
    } while (FindNextFile(hFind, &findFileData) != 0);

    *file_count = index;
    FindClose(hFind);

    // They aren't alphabetically sorted yet
    qsort(files, index, sizeof(char *), file_sorter);

    return files;

error_out:
    for (int i = 0; i < index; i++)
    {
        free(files[i]);
    }
    free(files);
    *file_count = 0;
    files = NULL;
    return NULL;
}

bool open_file_browser(char *selected_file, const char **files, const int file_count)
{
    ImVec2 win_size;
    ImGuiIO *ioptr = igGetIO();

    bool opened = true;

    selected_file[0] = '\0';

    int width = ioptr->DisplaySize.x - MARGIN * 2;

    igSetNextWindowPos((ImVec2){(ioptr->DisplaySize.x - width) / 2, 125}, ImGuiCond_None, (ImVec2){0, 0});
    igSetNextWindowSize((ImVec2){width, ioptr->DisplaySize.y - 175}, ImGuiCond_None);
    if (igBegin("browser", NULL, ImGuiWindowFlags_NoTitleBar))
    {
        igGetContentRegionAvail(&win_size);
        igPushStyleVar_Vec2(ImGuiStyleVar_ButtonTextAlign, (ImVec2){0.0f, 0.5f});
        for (int i = 0; i < file_count; i++)
        {
            if (igButton(files[i], (ImVec2){win_size.x, win_size.y / 7 - igGetStyle()->FramePadding.y}))
            {
                strcpy(selected_file, ROOT_DIR "\\");
                strcat(selected_file, files[i]);
                opened = false;
                break;
            }
        }
        igPopStyleVar(1);

        if (igIsKeyPressed_Bool(ImGuiKey_GamepadFaceRight, false))
        {
            selected_file[0] = '\0';
            opened = false;
        }
    }
    igEnd();
    return opened;
}
