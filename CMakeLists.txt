cmake_minimum_required(VERSION 3.7)
project(xenium_tools)

# Change these for your own project
set(LVGL_DIR "${CMAKE_SOURCE_DIR}/lvgl-sdl/lvgl")
set(LVDRV_DIR "${CMAKE_SOURCE_DIR}/lvgl-sdl/lv_drv")
set(PROJECT_DIR "${CMAKE_SOURCE_DIR}")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS_RELEASE "-O2")

# Add all the source files
file(GLOB LVGL_SOURCE_FILES
    # My lvgl driver source files
    "${LVDRV_DIR}/lv_sdl_drv_input.c"
    "${LVDRV_DIR}/lv_sdl_drv_display.c"
    "${LVDRV_DIR}/lv_if_drv_filesystem.c"

    # Main lvgl source files
    "${LVGL_DIR}/src/lv_core/*.c"
    "${LVGL_DIR}/src/lv_draw/*.c"
    "${LVGL_DIR}/src/lv_font/*.c"
    "${LVGL_DIR}/src/lv_gpu/*.c"
    "${LVGL_DIR}/src/lv_hal/*.c"
    "${LVGL_DIR}/src/lv_misc/*.c"
    "${LVGL_DIR}/src/lv_themes/*.c"
    "${LVGL_DIR}/src/lv_widgets/*.c"

    # xenium_tools sources files
    "${PROJECT_DIR}/main.c"
    "${PROJECT_DIR}/menu.c"
    "${PROJECT_DIR}/resources/xenium_tools_header.c"
    "${PROJECT_DIR}/xenium/xenium.c"
)
add_executable(xenium_tools ${LVGL_SOURCE_FILES})

# Add SDL2 lib
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})
target_link_libraries(xenium_tools ${SDL2_LIBRARY})

# Xenium-tools header includes
include_directories(${PROJECT_DIR})
include_directories(${PROJECT_DIR}/xenium)

# My lvgl driver includes
include_directories(${LVDRV_DIR})

# Main lvgl lib includes
include_directories(${LVGL_DIR}/src/)
include_directories(${LVGL_DIR}/../)
