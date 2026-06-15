cmake_minimum_required(VERSION 3.20)

project(MuPDFExternal LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

set(MUPDF_SOURCE_DIR "D:/MyProjects/MyQt/mupdf-src" CACHE PATH "Path to MuPDF source tree")
set(FREETYPE_ROOT "C:/Packages/freetype_2.14.3-msvc-x64" CACHE PATH "Path to custom FreeType package")
set(ZLIB_ROOT "C:/Packages/zlib_1.3.2-msvc-x64" CACHE PATH "Path to custom zlib package")
set(PNG_ROOT "C:/Packages/libpng_1.6.56-msvc-x64" CACHE PATH "Path to custom libpng package")

option(MUPDF_BUILD_MUTOOL "Build mutool executable" ON)
option(MUPDF_ENABLE_XPS "Enable XPS support" OFF)
option(MUPDF_ENABLE_SVG "Enable SVG support" ON)
option(MUPDF_ENABLE_HTML "Enable HTML/EPUB/FB2/MOBI/TXT support" ON)
option(MUPDF_ENABLE_JS "Enable JavaScript support" ON)
option(MUPDF_ENABLE_BROTLI "Enable Brotli support" ON)
option(MUPDF_ENABLE_DOCX_OUTPUT "Enable DOCX/ODT output helpers" OFF)
option(MUPDF_ENABLE_BARCODE "Enable barcode support" OFF)
option(MUPDF_ENABLE_TESSERACT "Enable OCR support" OFF)
option(MUPDF_ENABLE_LIBARCHIVE "Enable libarchive support" OFF)
option(MUPDF_ENABLE_PKCS7_OPENSSL "Enable OpenSSL-backed PKCS7 helpers" OFF)
option(MUPDF_USE_TOFU_FONTS "Use reduced built-in font set to avoid generating extra embedded font objects" ON)

if(NOT EXISTS "${MUPDF_SOURCE_DIR}/include/mupdf/fitz.h")
    message(FATAL_ERROR "MUPDF_SOURCE_DIR does not look like a MuPDF source tree: ${MUPDF_SOURCE_DIR}")
endif()

if(MUPDF_ENABLE_XPS AND NOT EXISTS "${MUPDF_SOURCE_DIR}/thirdparty/jpegxr/Software")
    message(FATAL_ERROR "MUPDF_ENABLE_XPS=ON requires ${MUPDF_SOURCE_DIR}/thirdparty/jpegxr/Software, but that directory is missing in this checkout.")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake;${CMAKE_MODULE_PATH}")

set(ZLIB_DIR "${ZLIB_ROOT}/lib/cmake/zlib")
set(freetype_DIR "${FREETYPE_ROOT}/lib/cmake/freetype")
set(PNG_DIR "${PNG_ROOT}/lib/cmake/PNG")

find_package(ZLIB CONFIG REQUIRED)
find_package(PNG CONFIG REQUIRED)
find_package(freetype CONFIG REQUIRED)

if(NOT TARGET ZLIB::ZLIB)
    message(FATAL_ERROR "Custom zlib package did not provide ZLIB::ZLIB")
endif()

if(NOT TARGET Freetype::Freetype)
    message(FATAL_ERROR "Custom FreeType package did not provide Freetype::Freetype")
endif()

function(mupdf_collect_glob out_var)
    set(result)
    foreach(pattern IN LISTS ARGN)
        file(GLOB files CONFIGURE_DEPENDS "${pattern}")
        list(APPEND result ${files})
    endforeach()
    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

set(MUPDF_PUBLIC_INCLUDE_DIRS
    "${MUPDF_SOURCE_DIR}/include"
    "${MUPDF_SOURCE_DIR}/generated"
)

set(MUPDF_THIRD_INCLUDE_DIRS
    "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/include"
    "${MUPDF_SOURCE_DIR}/thirdparty/gumbo-parser/src"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src"
    "${MUPDF_SOURCE_DIR}/thirdparty/jbig2dec"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg"
    "${MUPDF_SOURCE_DIR}/scripts/libjpeg"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/include"
    "${MUPDF_SOURCE_DIR}/thirdparty/mujs"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2"
)

if(WIN32)
    list(APPEND MUPDF_THIRD_INCLUDE_DIRS
        "${MUPDF_SOURCE_DIR}/thirdparty/gumbo-parser/visualc/include"
    )
endif()

if(NOT MUPDF_ENABLE_DOCX_OUTPUT)
    list(APPEND MUPDF_PUBLIC_DEFINES FZ_ENABLE_DOCX_OUTPUT=0)
endif()

if(NOT MUPDF_ENABLE_BARCODE)
    list(APPEND MUPDF_PUBLIC_DEFINES FZ_ENABLE_BARCODE=0)
endif()

if(NOT MUPDF_ENABLE_JS)
    list(APPEND MUPDF_PUBLIC_DEFINES FZ_ENABLE_JS=0)
endif()

if(NOT MUPDF_ENABLE_BROTLI)
    list(APPEND MUPDF_PUBLIC_DEFINES FZ_ENABLE_BROTLI=0)
endif()

if(NOT MUPDF_ENABLE_XPS)
    list(APPEND MUPDF_PUBLIC_DEFINES FZ_ENABLE_XPS=0)
endif()

if(NOT MUPDF_ENABLE_SVG)
    list(APPEND MUPDF_PUBLIC_DEFINES FZ_ENABLE_SVG=0)
endif()

if(NOT MUPDF_ENABLE_HTML)
    list(APPEND MUPDF_PUBLIC_DEFINES
        FZ_ENABLE_HTML_ENGINE=0
        FZ_ENABLE_HTML=0
        FZ_ENABLE_EPUB=0
        FZ_ENABLE_FB2=0
        FZ_ENABLE_MOBI=0
        FZ_ENABLE_OFFICE=0
        FZ_ENABLE_TXT=0
    )
endif()

if(MUPDF_USE_TOFU_FONTS)
    list(APPEND MUPDF_PUBLIC_DEFINES TOFU TOFU_CJK TOFU_CJK_EXT TOFU_CJK_LANG)
endif()

set(MUPDF_WIN_LIBS)
if(WIN32)
    list(APPEND MUPDF_PUBLIC_DEFINES WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        list(APPEND MUPDF_PUBLIC_DEFINES WIN64)
    endif()
    list(APPEND MUPDF_WIN_LIBS ws2_32 bcrypt)
endif()

mupdf_collect_glob(MUPDF_FITZ_C_SOURCES "${MUPDF_SOURCE_DIR}/source/fitz/*.c")
mupdf_collect_glob(MUPDF_FITZ_CPP_SOURCES "${MUPDF_SOURCE_DIR}/source/fitz/*.cpp")
mupdf_collect_glob(MUPDF_PDF_SOURCES "${MUPDF_SOURCE_DIR}/source/pdf/*.c")
mupdf_collect_glob(MUPDF_REFLOW_SOURCES "${MUPDF_SOURCE_DIR}/source/reflow/*.c")
mupdf_collect_glob(MUPDF_CBZ_SOURCES "${MUPDF_SOURCE_DIR}/source/cbz/*.c")
mupdf_collect_glob(MUPDF_XPS_SOURCES "${MUPDF_SOURCE_DIR}/source/xps/*.c")
mupdf_collect_glob(MUPDF_SVG_SOURCES "${MUPDF_SOURCE_DIR}/source/svg/*.c")
mupdf_collect_glob(MUPDF_HTML_SOURCES "${MUPDF_SOURCE_DIR}/source/html/*.c")

set(MUPDF_CORE_SOURCES
    ${MUPDF_FITZ_C_SOURCES}
    ${MUPDF_FITZ_CPP_SOURCES}
    ${MUPDF_PDF_SOURCES}
    ${MUPDF_REFLOW_SOURCES}
    ${MUPDF_CBZ_SOURCES}
    "${MUPDF_SOURCE_DIR}/source/helpers/pkcs7/pkcs7-openssl.c"
)

set(MUPDF_THREAD_HELPER_SOURCE
    "${MUPDF_SOURCE_DIR}/source/helpers/mu-threads/mu-threads.c"
)

if(MUPDF_ENABLE_XPS)
    list(APPEND MUPDF_CORE_SOURCES ${MUPDF_XPS_SOURCES})
endif()

if(MUPDF_ENABLE_SVG)
    list(APPEND MUPDF_CORE_SOURCES ${MUPDF_SVG_SOURCES})
endif()

if(MUPDF_ENABLE_HTML)
    list(APPEND MUPDF_CORE_SOURCES ${MUPDF_HTML_SOURCES})
endif()

set(MUPDF_GENERATED_SOURCES
    "${MUPDF_SOURCE_DIR}/generated/resources/hyphen/hyph-all.zip.c"
    "${MUPDF_SOURCE_DIR}/generated/resources/fonts/urw/Dingbats.cff.c"
    "${MUPDF_SOURCE_DIR}/generated/resources/fonts/urw/NimbusBoxes-Regular.cff.c"
    "${MUPDF_SOURCE_DIR}/generated/resources/fonts/urw/NimbusMonoPS-Bold.cff.c"
    "${MUPDF_SOURCE_DIR}/generated/resources/fonts/urw/NimbusMonoPS-BoldItalic.cff.c"
    "${MUPDF_SOURCE_DIR}/generated/resources/fonts/urw/NimbusMonoPS-Italic.cff.c"
    "${MUPDF_SOURCE_DIR}/generated/resources/fonts/urw/NimbusMonoPS-Regular.cff.c"
    "${MUPDF_SOURCE_DIR}/generated/resources/fonts/urw/NimbusRoman-Bold.cff.c"
    "${MUPDF_SOURCE_DIR}/generated/resources/fonts/urw/NimbusRoman-BoldItalic.cff.c"
    "${MUPDF_SOURCE_DIR}/generated/resources/fonts/urw/NimbusRoman-Italic.cff.c"
    "${MUPDF_SOURCE_DIR}/generated/resources/fonts/urw/NimbusRoman-Regular.cff.c"
    "${MUPDF_SOURCE_DIR}/generated/resources/fonts/urw/NimbusSans-Bold.cff.c"
    "${MUPDF_SOURCE_DIR}/generated/resources/fonts/urw/NimbusSans-BoldItalic.cff.c"
    "${MUPDF_SOURCE_DIR}/generated/resources/fonts/urw/NimbusSans-Italic.cff.c"
    "${MUPDF_SOURCE_DIR}/generated/resources/fonts/urw/NimbusSans-Regular.cff.c"
    "${MUPDF_SOURCE_DIR}/generated/resources/fonts/urw/StandardSymbolsPS.cff.c"
)

set(MUPDF_THIRD_C_SOURCES
    "${MUPDF_SOURCE_DIR}/thirdparty/gumbo-parser/src/attribute.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/gumbo-parser/src/char_ref.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/gumbo-parser/src/error.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/gumbo-parser/src/parser.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/gumbo-parser/src/string_buffer.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/gumbo-parser/src/string_piece.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/gumbo-parser/src/tag.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/gumbo-parser/src/tokenizer.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/gumbo-parser/src/utf8.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/gumbo-parser/src/util.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/gumbo-parser/src/vector.c"

    "${MUPDF_SOURCE_DIR}/thirdparty/jbig2dec/jbig2.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/jbig2dec/jbig2_arith.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/jbig2dec/jbig2_arith_iaid.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/jbig2dec/jbig2_arith_int.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/jbig2dec/jbig2_generic.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/jbig2dec/jbig2_halftone.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/jbig2dec/jbig2_huffman.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/jbig2dec/jbig2_hufftab.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/jbig2dec/jbig2_image.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/jbig2dec/jbig2_mmr.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/jbig2dec/jbig2_page.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/jbig2dec/jbig2_refinement.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/jbig2dec/jbig2_segment.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/jbig2dec/jbig2_symbol_dict.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/jbig2dec/jbig2_text.c"

    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jaricom.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jcapimin.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jcapistd.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jcarith.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jccoefct.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jccolor.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jcdctmgr.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jchuff.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jcinit.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jcmainct.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jcmarker.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jcmaster.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jcomapi.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jcparam.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jcprepct.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jcsample.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jdapimin.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jdapistd.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jdarith.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jdatadst.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jdatasrc.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jdcoefct.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jdcolor.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jddctmgr.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jdhuff.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jdinput.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jdmainct.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jdmarker.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jdmaster.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jdmerge.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jdpostct.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jdsample.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jdtrans.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jerror.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jfdctflt.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jfdctfst.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jfdctint.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jidctflt.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jidctfst.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jidctint.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jmemmgr.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jquant1.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jquant2.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/libjpeg/jutils.c"

    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmsalpha.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmscam02.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmscgats.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmscnvrt.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmserr.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmsgamma.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmsgmt.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmshalf.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmsintrp.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmsio0.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmsio1.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmslut.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmsmd5.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmsmtrx.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmsnamed.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmsopt.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmspack.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmspcs.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmsplugin.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmsps2.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmssamp.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmssm.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmstypes.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmsvirt.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmswtpnt.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/lcms2/src/cmsxform.c"

    "${MUPDF_SOURCE_DIR}/thirdparty/mujs/one.c"

    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/bio.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/cio.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/dwt.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/event.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/function_list.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/ht_dec.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/image.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/invert.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/j2k.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/jp2.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/mct.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/mqc.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/openjpeg.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/pi.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/sparse_array.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/t1.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/t2.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/tcd.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/tgt.c"
    "${MUPDF_SOURCE_DIR}/thirdparty/openjpeg/src/lib/openjp2/thread.c"
)

set(MUPDF_THIRD_CPP_SOURCES
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/graph/gsubgpos-context.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-aat-layout.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-aat-map.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-blob.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-buffer.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-buffer-verify.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-buffer-serialize.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-common.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-face.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-fallback-shape.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-font.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ft.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-map.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-number.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-cff1-table.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-cff2-table.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-color.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-face.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-font.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-layout.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-map.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-math.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-meta.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-metrics.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-name.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-shape.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-shape-fallback.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-shape-normalize.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-shaper-arabic.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-shaper-default.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-shaper-hangul.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-shaper-hebrew.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-shaper-indic.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-shaper-indic-table.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-shaper-khmer.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-shaper-myanmar.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-shaper-syllabic.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-shaper-thai.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-shaper-use.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-shaper-vowel-constraints.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-tag.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ot-var.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-set.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-shape.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-shape-plan.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-shaper.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-static.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-subset.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-subset-cff1.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-subset-cff2.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-subset-cff-common.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-subset-input.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-subset-plan.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-ucd.cc"
    "${MUPDF_SOURCE_DIR}/thirdparty/harfbuzz/src/hb-unicode.cc"
)

if(MUPDF_ENABLE_BROTLI)
    list(APPEND MUPDF_THIRD_C_SOURCES
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/common/constants.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/common/context.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/common/dictionary.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/common/platform.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/common/shared_dictionary.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/common/transform.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/dec/bit_reader.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/dec/decode.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/dec/huffman.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/dec/state.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/backward_references.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/backward_references_hq.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/bit_cost.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/block_splitter.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/brotli_bit_stream.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/cluster.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/command.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/compound_dictionary.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/compress_fragment.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/compress_fragment_two_pass.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/dictionary_hash.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/encode.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/encoder_dict.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/entropy_encode.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/fast_log.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/histogram.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/literal_cost.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/memory.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/metablock.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/static_dict.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/brotli/c/enc/utf8_util.c"
    )
endif()

if(MUPDF_ENABLE_XPS)
    list(APPEND MUPDF_PUBLIC_DEFINES HAVE_JPEGXR)
    list(APPEND MUPDF_THIRD_INCLUDE_DIRS
        "${MUPDF_SOURCE_DIR}/thirdparty/jpegxr"
        "${MUPDF_SOURCE_DIR}/thirdparty/jpegxr/Software"
    )
    list(APPEND MUPDF_THIRD_C_SOURCES
        "${MUPDF_SOURCE_DIR}/thirdparty/jpegxr/Software/algo.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/jpegxr/Software/api.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/jpegxr/Software/cr_parse.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/jpegxr/Software/flags.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/jpegxr/Software/init.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/jpegxr/Software/io.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/jpegxr/Software/jpegxr_pixelformat.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/jpegxr/Software/r_parse.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/jpegxr/Software/r_strip.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/jpegxr/Software/r_tile_frequency.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/jpegxr/Software/r_tile_spatial.c"
        "${MUPDF_SOURCE_DIR}/thirdparty/jpegxr/Software/x_strip.c"
    )
endif()

add_library(mupdf STATIC
    ${MUPDF_CORE_SOURCES}
    ${MUPDF_GENERATED_SOURCES}
    ${MUPDF_THIRD_C_SOURCES}
    ${MUPDF_THIRD_CPP_SOURCES}
)

target_include_directories(mupdf
    PUBLIC
        ${MUPDF_PUBLIC_INCLUDE_DIRS}
        ${MUPDF_THIRD_INCLUDE_DIRS}
)

target_compile_definitions(mupdf
    PUBLIC
        ${MUPDF_PUBLIC_DEFINES}
        OPJ_STATIC
        HAVE_LCMS2MT=1
        LCMS2MT_PREFIX=lcms2mt_
    PRIVATE
        HAVE_FALLBACK=1
        HAVE_FREETYPE
        HAVE_OT
        HAVE_ROUND
        HAVE_UCDN
        HB_NO_MT
        HB_NO_PRAGMA_GCC_DIAGNOSTIC
        hb_malloc_impl=fz_hb_malloc
        hb_calloc_impl=fz_hb_calloc
        hb_free_impl=fz_hb_free
        hb_realloc_impl=fz_hb_realloc
        HAVE_STDINT_H
        JBIG_EXTERNAL_MEMENTO_H=\"mupdf/memento.h\"
        MUTEX_pthread=0
)

target_compile_options(mupdf
    PRIVATE
        $<$<COMPILE_LANGUAGE:C>:
            $<$<C_COMPILER_ID:MSVC>:/wd4244;/wd4701;/wd4996;/utf-8>
            $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wno-unused-function>
        >
        $<$<COMPILE_LANGUAGE:CXX>:
            $<$<CXX_COMPILER_ID:MSVC>:/EHsc;/wd4244;/wd4701;/wd4996;/utf-8>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-exceptions;-fno-rtti;-fno-threadsafe-statics;-fvisibility-inlines-hidden;-w>
        >
)

target_link_libraries(mupdf
    PUBLIC
        ZLIB::ZLIB
        Freetype::Freetype
        ${MUPDF_WIN_LIBS}
)

source_group(TREE "${MUPDF_SOURCE_DIR}" FILES
    ${MUPDF_CORE_SOURCES}
    ${MUPDF_GENERATED_SOURCES}
    ${MUPDF_THIRD_C_SOURCES}
    ${MUPDF_THIRD_CPP_SOURCES}
)

if(MUPDF_BUILD_MUTOOL)
    set(MUTOOL_SOURCES
        "${MUPDF_SOURCE_DIR}/source/tools/mutool.c"
        "${MUPDF_SOURCE_DIR}/source/tools/muconvert.c"
        "${MUPDF_SOURCE_DIR}/source/tools/mudraw.c"
        "${MUPDF_SOURCE_DIR}/source/tools/murun.c"
        "${MUPDF_SOURCE_DIR}/source/tools/mutrace.c"
        "${MUPDF_SOURCE_DIR}/source/tools/mugrep.c"
        "${MUPDF_SOURCE_DIR}/source/tools/cmapdump.c"
        "${MUPDF_SOURCE_DIR}/source/tools/pdfaudit.c"
        "${MUPDF_SOURCE_DIR}/source/tools/pdfbake.c"
        "${MUPDF_SOURCE_DIR}/source/tools/pdfclean.c"
        "${MUPDF_SOURCE_DIR}/source/tools/pdfcreate.c"
        "${MUPDF_SOURCE_DIR}/source/tools/pdfextract.c"
        "${MUPDF_SOURCE_DIR}/source/tools/pdfinfo.c"
        "${MUPDF_SOURCE_DIR}/source/tools/pdfmerge.c"
        "${MUPDF_SOURCE_DIR}/source/tools/pdfpages.c"
        "${MUPDF_SOURCE_DIR}/source/tools/pdfposter.c"
        "${MUPDF_SOURCE_DIR}/source/tools/pdfrecolor.c"
        "${MUPDF_SOURCE_DIR}/source/tools/pdfshow.c"
        "${MUPDF_SOURCE_DIR}/source/tools/pdfsign.c"
        "${MUPDF_SOURCE_DIR}/source/tools/pdftrim.c"
        ${MUPDF_THREAD_HELPER_SOURCE}
    )

    if(MUPDF_ENABLE_BARCODE)
        list(APPEND MUTOOL_SOURCES "${MUPDF_SOURCE_DIR}/source/tools/mubar.c")
    endif()

    add_executable(mutool ${MUTOOL_SOURCES})
    target_include_directories(mutool PRIVATE
        ${MUPDF_PUBLIC_INCLUDE_DIRS}
        "${MUPDF_SOURCE_DIR}/thirdparty/mujs"
    )
    target_compile_definitions(mutool PRIVATE ${MUPDF_PUBLIC_DEFINES})
    target_compile_options(mutool PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/utf-8>
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    )
    target_link_libraries(mutool PRIVATE mupdf)

    if(MSVC)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(MUPDF_MSVC_WSETARGV_OBJ "$ENV{VCToolsInstallDir}lib\\x64\\wsetargv.obj")
        else()
            set(MUPDF_MSVC_WSETARGV_OBJ "$ENV{VCToolsInstallDir}lib\\x86\\wsetargv.obj")
        endif()
        if(EXISTS "${MUPDF_MSVC_WSETARGV_OBJ}")
            target_sources(mutool PRIVATE "${MUPDF_MSVC_WSETARGV_OBJ}")
        else()
            message(WARNING "wsetargv.obj not found at ${MUPDF_MSVC_WSETARGV_OBJ}; mutool may fail to link on Windows.")
        endif()
    endif()
endif()

message(STATUS "MuPDF source dir: ${MUPDF_SOURCE_DIR}")
message(STATUS "Using custom FreeType from: ${FREETYPE_ROOT}")
message(STATUS "Using custom zlib from: ${ZLIB_ROOT}")
message(STATUS "Using custom libpng from: ${PNG_ROOT}")
