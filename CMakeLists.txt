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

add_library(layout_core STATIC
    src/layout/layout.c
    src/layout/layout_tree.c
    src/layout/layout_tree_builder.c
    src/layout/layout_tree_traversal.c
    src/layout/layout_algorithm.c
)
target_include_directories(layout_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(layout_core PUBLIC Lexbor::Lexbor)

add_library(qt_adaptor STATIC
    src/qt_adaptor/layout_scene_adapter.cpp
)
target_include_directories(qt_adaptor PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(qt_adaptor PUBLIC
    Qt6::Quick
    layout_core
)

# End-to-end smoke test for Lexbor computed style (Lexbor-only, no Qt).
add_executable(style_smoke tests/style_smoke.cpp)
target_link_libraries(style_smoke PRIVATE Lexbor::Lexbor)

add_executable(layout_smoke tests/layout_smoke.c)
target_link_libraries(layout_smoke PRIVATE layout_core)

add_executable(qquickitem_embed_smoke tests/qquickitem_embed_smoke.cpp)
target_link_libraries(qquickitem_embed_smoke PRIVATE
    Qt6::Gui
    Qt6::Quick
    layout_core
    Lexbor::Lexbor
)

add_executable(layout_scene_adapter_smoke tests/layout_scene_adapter_smoke.cpp)
target_link_libraries(layout_scene_adapter_smoke PRIVATE
    Qt6::Gui
    qt_adaptor
    layout_core
)

add_executable(main
    src/main.cpp
)

target_link_libraries(main PRIVATE
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    MuPDF::MuPDF
    Lexbor::Lexbor
    layout_core
    qt_adaptor
    PNG::PNG
    ZLIB::ZLIB
    "${FREETYPE_LINK_TARGET}"
    harfbuzz::harfbuzz
    ws2_32
    bcrypt
)

target_compile_definitions(main PRIVATE
    QT_QML_IMPORT_PATH="${Qt6_ROOT}/qml"
    QT_RUNTIME_DLL_PATH="${Qt6_ROOT}/bin"
)
