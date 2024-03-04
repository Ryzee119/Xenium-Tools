set (XBE_TITLE XeniumTools)
set (XBOX_ISO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/XeniumTools)

cmake_minimum_required(VERSION 3.14)
project(XeniumTools C CXX)
include(FindPkgConfig)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_C_FLAGS_RELEASE "-O2")

# Stop lots of warning spam
add_compile_options(-Wno-builtin-macro-redefined)
add_definitions(-DXBOX -DNXDK)

pkg_search_module(SDL2 REQUIRED sdl2)

# OpenGL
file(GLOB PBGL_SOURCES "lib/pbgl/src/*.c")
add_library(pbgl STATIC ${PBGL_SOURCES})
target_include_directories(pbgl PUBLIC "lib/pbgl/include")
target_sources(pbgl PRIVATE "glue/pbgl_stubs.c")
set(OPENGL_INCLUDE_DIRS "lib/pbgl/include")
set(OPENGL_LIBRARIES pbgl)

# IMGUI
set(IMGUI_STATIC "yes")
add_subdirectory(lib/cimgui)
target_sources(cimgui PRIVATE "glue/atof.c")
target_compile_options(cimgui PRIVATE -DIMGUI_DISABLE_WIN32_FUNCTIONS -DIMGUI_USE_STB_SPRINTF)
target_include_directories(cimgui PRIVATE "lib/stb_sprintf" "src/backend/xbox/glue")

# IMGUI BACKEND
add_library(cimgur_backend STATIC "lib/cimgui/imgui/backends/imgui_impl_opengl2.cpp" "glue/imgui_impl_sdl2.cpp")
target_link_libraries(cimgur_backend cimgui ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES})

# TLSF memory allocator
add_library(tlsf STATIC "lib/tlsf/tlsf.c")

# TLSF memory allocator
add_library(xenium STATIC "lib/xenium/xenium.c")

set(LX_SOURCES 
    main.c
)

add_executable(XeniumTools
    ${LX_SOURCES}
)

target_compile_definitions(XeniumTools PRIVATE "")
include_directories(XeniumTools ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} lib)
target_link_libraries(XeniumTools PUBLIC ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} ${NXDK_DIR}/lib/libpbkit.lib cimgur_backend cimgui tlsf xenium)
target_link_libraries(XeniumTools PUBLIC ${SDL2_LDFLAGS} ${OPENGL_LDFLAGS})

if (NXDK)
#Post-build exe to xbe conversion
add_custom_target(cxbe_convert ALL
    VERBATIM COMMAND "${CMAKE_COMMAND}" -E env ./tools/cxbe/cxbe -OUT:${XBOX_ISO_DIR}/default.xbe -TITLE:${XBE_TITLE} ${CMAKE_CURRENT_BINARY_DIR}/${XBE_TITLE}.exe
    WORKING_DIRECTORY ${NXDK_DIR}
    COMMENT "CXBE Conversion: [EXE -> XBE]"
)
add_dependencies(cxbe_convert XeniumTools)

#Post-build xbe to xiso conversion
add_custom_target(xbe_iso ALL
    VERBATIM COMMAND "${CMAKE_COMMAND}" -E env ${NXDK_DIR}/tools/extract-xiso/build/extract-xiso -c ${XBOX_ISO_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "CXBE Conversion: [XBE -> XISO]"
)
add_dependencies(xbe_iso cxbe_convert)

set_target_properties(cxbe_convert PROPERTIES OUTPUT_QUIET ON)
set_target_properties(xbe_iso PROPERTIES OUTPUT_QUIET ON)
endif()
