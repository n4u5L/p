find_path(MuPDF_INCLUDE_DIR
    NAMES mupdf/fitz.h
    HINTS
        ${MuPDF_ROOT}
        $ENV{MuPDF_ROOT}
        ${MUPDF_ROOT}
        $ENV{MUPDF_ROOT}
    PATH_SUFFIXES
        include
)

find_library(MuPDF_LIBRARY_RELEASE
    NAMES mupdf
    HINTS
        ${MuPDF_ROOT}
        $ENV{MuPDF_ROOT}
        ${MUPDF_ROOT}
        $ENV{MUPDF_ROOT}
        ${MuPDF_BUILD_ROOT}
        $ENV{MuPDF_BUILD_ROOT}
        ${MUPDF_BUILD_ROOT}
        $ENV{MUPDF_BUILD_ROOT}
    PATH_SUFFIXES
        Release
        release
        lib
)

find_library(MuPDF_LIBRARY_DEBUG
    NAMES mupdfd mupdf
    HINTS
        ${MuPDF_ROOT}
        $ENV{MuPDF_ROOT}
        ${MUPDF_ROOT}
        $ENV{MUPDF_ROOT}
        ${MuPDF_BUILD_ROOT}
        $ENV{MuPDF_BUILD_ROOT}
        ${MUPDF_BUILD_ROOT}
        $ENV{MUPDF_BUILD_ROOT}
    PATH_SUFFIXES
        Debug
        debug
        lib
)

include(SelectLibraryConfigurations)
select_library_configurations(MuPDF)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MuPDF
    REQUIRED_VARS MuPDF_INCLUDE_DIR MuPDF_LIBRARY
)

if(MuPDF_FOUND AND NOT TARGET MuPDF::MuPDF)
    add_library(MuPDF::MuPDF UNKNOWN IMPORTED)
    set_target_properties(MuPDF::MuPDF PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${MuPDF_INCLUDE_DIR}"
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    )

    if(MuPDF_LIBRARY_RELEASE)
        set_property(TARGET MuPDF::MuPDF APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(MuPDF::MuPDF PROPERTIES
            IMPORTED_LOCATION_RELEASE "${MuPDF_LIBRARY_RELEASE}"
        )
    endif()

    if(MuPDF_LIBRARY_DEBUG)
        set_property(TARGET MuPDF::MuPDF APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(MuPDF::MuPDF PROPERTIES
            IMPORTED_LOCATION_DEBUG "${MuPDF_LIBRARY_DEBUG}"
        )
    endif()

    if(MuPDF_LIBRARY)
        set_target_properties(MuPDF::MuPDF PROPERTIES
            IMPORTED_LOCATION "${MuPDF_LIBRARY}"
        )
    endif()
endif()

mark_as_advanced(
    MuPDF_INCLUDE_DIR
    MuPDF_LIBRARY
    MuPDF_LIBRARY_RELEASE
    MuPDF_LIBRARY_DEBUG
)
