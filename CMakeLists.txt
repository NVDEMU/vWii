cmake_minimum_required(VERSION 3.24)

project(vWii
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Wii emulator for Windows and macOS"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VWII_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

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
    src/video/video_backend.cpp
)

target_include_directories(vwii_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

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
