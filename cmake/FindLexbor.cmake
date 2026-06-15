set(_Lexbor_DEFAULT_ROOT "D:/MyProjects/MyQt/lexbor_3.0.0-src")
set(_Lexbor_DEFAULT_BUILD_ROOT "${_Lexbor_DEFAULT_ROOT}/build")

find_path(Lexbor_INCLUDE_DIR
    NAMES lexbor/html/html.h
    HINTS
        ${Lexbor_ROOT}
        $ENV{Lexbor_ROOT}
        ${LEXBOR_ROOT}
        $ENV{LEXBOR_ROOT}
        ${_Lexbor_DEFAULT_ROOT}
    PATH_SUFFIXES
        source
        include
)

find_library(Lexbor_LIBRARY_RELEASE
    NAMES lexbor_static lexbor
    HINTS
        ${Lexbor_ROOT}
        $ENV{Lexbor_ROOT}
        ${LEXBOR_ROOT}
        $ENV{LEXBOR_ROOT}
        ${Lexbor_BUILD_ROOT}
        $ENV{Lexbor_BUILD_ROOT}
        ${LEXBOR_BUILD_ROOT}
        $ENV{LEXBOR_BUILD_ROOT}
        ${_Lexbor_DEFAULT_BUILD_ROOT}
        ${_Lexbor_DEFAULT_ROOT}
    PATH_SUFFIXES
        Release
        release
        lib
        build/Release
        build/release
)

find_library(Lexbor_LIBRARY_DEBUG
    NAMES lexbor_static lexbor
    HINTS
        ${Lexbor_ROOT}
        $ENV{Lexbor_ROOT}
        ${LEXBOR_ROOT}
        $ENV{LEXBOR_ROOT}
        ${Lexbor_BUILD_ROOT}
        $ENV{Lexbor_BUILD_ROOT}
        ${LEXBOR_BUILD_ROOT}
        $ENV{LEXBOR_BUILD_ROOT}
        ${_Lexbor_DEFAULT_BUILD_ROOT}
        ${_Lexbor_DEFAULT_ROOT}
    PATH_SUFFIXES
        Debug
        debug
        lib
        build/Debug
        build/debug
)

include(SelectLibraryConfigurations)
select_library_configurations(Lexbor)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Lexbor
    REQUIRED_VARS Lexbor_INCLUDE_DIR Lexbor_LIBRARY
)

if(Lexbor_FOUND AND NOT TARGET Lexbor::Lexbor)
    add_library(Lexbor::Lexbor UNKNOWN IMPORTED)
    set_target_properties(Lexbor::Lexbor PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${Lexbor_INCLUDE_DIR}"
        INTERFACE_COMPILE_DEFINITIONS "LEXBOR_STATIC"
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    )

    if(Lexbor_LIBRARY_RELEASE)
        set_property(TARGET Lexbor::Lexbor APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(Lexbor::Lexbor PROPERTIES
            IMPORTED_LOCATION_RELEASE "${Lexbor_LIBRARY_RELEASE}"
        )
    endif()

    if(Lexbor_LIBRARY_DEBUG)
        set_property(TARGET Lexbor::Lexbor APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(Lexbor::Lexbor PROPERTIES
            IMPORTED_LOCATION_DEBUG "${Lexbor_LIBRARY_DEBUG}"
        )
    endif()

    if(Lexbor_LIBRARY)
        set_target_properties(Lexbor::Lexbor PROPERTIES
            IMPORTED_LOCATION "${Lexbor_LIBRARY}"
        )
    endif()
endif()

mark_as_advanced(
    Lexbor_INCLUDE_DIR
    Lexbor_LIBRARY
    Lexbor_LIBRARY_RELEASE
    Lexbor_LIBRARY_DEBUG
)

unset(_Lexbor_DEFAULT_ROOT)
unset(_Lexbor_DEFAULT_BUILD_ROOT)
