cmake_minimum_required(VERSION 3.24)

project(vWii
    VERSION 0.3.0
    LANGUAGES C CXX
    DESCRIPTION "Wii emulator for Windows and macOS"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VWII_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)

include(FetchContent)

FetchContent_Declare(
    zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG v1.5.7
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR build/cmake
)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.16
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(zstd SDL3)

if(MSVC)
    add_compile_options(/W4)
    if(VWII_WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(VWII_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
endif()

add_library(vwii_core
    src/core/emulator.cpp
    src/cpu/powerpc.cpp
    src/memory/memory.cpp
    src/boot/image_loader.cpp
    src/boot/wii_game.cpp
    src/disc/disc_image.cpp
    src/hardware/hollywood.cpp
    src/hardware/video_interface.cpp
    src/hardware/gx_fifo.cpp
    src/hardware/audio_interface.cpp
    src/system/scheduler.cpp
    src/ios/ios_hle.cpp
    src/ios/nand_fs.cpp
    src/input/wiimote_keyboard.cpp
    src/filesystem/wii_fst.cpp
    src/frontend/frontend.cpp
    src/frontend/keyboard_bindings.cpp
    src/video/video_backend.cpp
)

target_include_directories(vwii_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

if(TARGET libzstd_static)
    set(VWII_ZSTD_TARGET libzstd_static)
elseif(TARGET zstd::libzstd_static)
    set(VWII_ZSTD_TARGET zstd::libzstd_static)
else()
    message(FATAL_ERROR "Zstandard static target was not created")
endif()

target_link_libraries(vwii_core PRIVATE ${VWII_ZSTD_TARGET} SDL3::SDL3)

target_compile_definitions(vwii_core
    PUBLIC
        VWII_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        VWII_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        VWII_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

add_executable(vwii
    src/main.cpp
)

target_link_libraries(vwii PRIVATE vwii_core SDL3::SDL3)

include(CTest)
if(BUILD_TESTING)
    add_executable(vwii_core_tests
        tests/core_tests.cpp
    )
    target_link_libraries(vwii_core_tests PRIVATE vwii_core)
    add_test(NAME vwii_core_tests COMMAND vwii_core_tests)
endif()

if(APPLE)
    set_target_properties(vwii PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "vWii"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.nvdemu.vwii"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()
