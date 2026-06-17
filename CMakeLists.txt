cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

project(MyProject LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(QT_NO_PRIVATE_MODULE_WARNING ON)

set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
set(Qt6_ROOT "D:/MyProjects/MyQt/Qt_6.11.1-src/build/qtbase")
set(ICU_ROOT "C:/Packages/icu_78.3-msvc-x64")
set(harfbuzz_DIR "C:/Packages/harfbuzz_14.2.0-msvc-x64/lib/cmake/harfbuzz")
set(ZLIB_DIR "C:/Packages/zlib_1.3.2-msvc-x64/lib/cmake/zlib")
set(PNG_DIR "C:/Packages/libpng_1.6.56-msvc-x64/lib/cmake/PNG")
set(freetype_DIR "C:/Packages/freetype_2.14.3-msvc-x64/lib/cmake/freetype")
set(MuPDF_ROOT "D:/MyProjects/MyQt/mupdf-src")
set(MuPDF_BUILD_ROOT "D:/MyProjects/MyQt/mupdf-src/build/Debug")
set(QT_NO_PRIVATE_MODULE_WARNING ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml)
find_package(ICU REQUIRED COMPONENTS uc data)
find_package(harfbuzz CONFIG REQUIRED)
find_package(ZLIB CONFIG REQUIRED)
find_package(PNG CONFIG REQUIRED)
find_package(freetype CONFIG REQUIRED)
find_package(MuPDF REQUIRED)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lexbor/CMakeLists.txt")
    set(LEXBOR_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(LEXBOR_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(LEXBOR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LEXBOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LEXBOR_BUILD_UTILS OFF CACHE BOOL "" FORCE)
    add_subdirectory(lexbor EXCLUDE_FROM_ALL)
    if(NOT TARGET Lexbor::Lexbor)
        add_library(Lexbor::Lexbor ALIAS lexbor_static)
    endif()
else()
    find_package(Lexbor REQUIRED)
endif()

add_library(MyOptions INTERFACE)
target_compile_options(MyOptions INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:-W4;-permissive-;-Zc:__cplusplus;/utf-8>
)
target_compile_definitions(MyOptions INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>)
link_libraries(MyOptions)

if(TARGET Freetype::Freetype)
    set(FREETYPE_LINK_TARGET Freetype::Freetype)
else()
    set(FREETYPE_LINK_TARGET freetype)
endif()

# Self-built style engine (the "second half" lexbor does not provide).
# Lexbor-only so it can be built/tested without the full Qt graphics stack.
add_library(style STATIC
    src/style/style_props.cpp
    src/style/length_resolve.cpp
    src/style/style_arena.cpp
    src/style/resolver.cpp
    src/style/style_engine.cpp
    src/style/ua_stylesheet.cpp
)
target_include_directories(style PUBLIC src/style)
target_link_libraries(style PUBLIC Lexbor::Lexbor)

# End-to-end smoke test for the style engine (Lexbor-only, no Qt).
add_executable(style_smoke tests/style_smoke.cpp)
target_link_libraries(style_smoke PRIVATE style Lexbor::Lexbor)

add_executable(main
)

target_link_libraries(main PRIVATE
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    MuPDF::MuPDF
    Lexbor::Lexbor
    PNG::PNG
    ZLIB::ZLIB
    "${FREETYPE_LINK_TARGET}"
    harfbuzz::harfbuzz
    ws2_32
    bcrypt
    style
)

target_compile_definitions(main PRIVATE
    QT_QML_IMPORT_PATH="${Qt6_ROOT}/qml"
    QT_RUNTIME_DLL_PATH="${Qt6_ROOT}/bin"
)
