cmake_minimum_required(VERSION 3.24)

project(vWii
    VERSION 0.2.0
    LANGUAGES CXX
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
)

FetchContent_MakeAvailable(zstd)

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

target_link_libraries(vwii_core PRIVATE ${VWII_ZSTD_TARGET})

target_compile_definitions(vwii_core
    PUBLIC
        VWII_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        VWII_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        VWII_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

add_executable(vwii
    src/main.cpp
)

target_link_libraries(vwii PRIVATE vwii_core)

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
    )
endif()
