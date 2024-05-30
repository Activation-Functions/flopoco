# Distributed under the FloPoCo License, see README.md for more information

#[=======================================================================[.rst:
FindWCPG
-------

Finds the WCPG library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``WCPG::WCPG``
  The WCPG library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``WCPG_FOUND``
  True if the system has the WCPG library.
``WCPG_VERSION``
  The version of the WCPG library which was found.
``WCPG_INCLUDE_DIRS``
  Include directories needed to use WCPG.
``WCPG_LIBRARIES``
  Libraries needed to link to WCPG.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``WCPG_INCLUDE_DIR``
  The directory containing ``wcpg.h``.
``WCPG_LIBRARY``
  The path to the WCPG library.

#]=======================================================================]

find_package(PkgConfig)
pkg_check_modules(PC_WCPG QUIET wcpg)

find_path(WCPG_INCLUDE_DIR
    NAMES wcpg.h
    PATHS ${PC_WCPG_INCLUDE_DIRS} ${WCPG_LOCAL}/include
    DOC "Path of wcpg.h, the include file for GNU WCPG library"
)

find_library(WCPG_LIBRARY
    NAMES wcpg
    PATHS ${PC_WCPG_LIBRARY_DIRS} ${WCPG_LOCAL}/lib
    DOC "Directory of the WCPG library"
)

set(WCPG_VERSION ${PC_WCPG_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    WCPG
    FOUND_VAR WCPG_FOUND
    REQUIRED_VARS
        WCPG_LIBRARY
        WCPG_INCLUDE_DIR
    VERSION_VAR WCPG_VERSION
)
if (WCPG_FOUND)
    set(WCPG_LIBRARIES ${WCPG_LIBRARY})
    set(WCPG_INCLUDE_DIRS ${WCPG_INCLUDE_DIR})
    set(WCPG_DEFINITIONS ${PC_WCPG_FLAGS_OTHER})
endif()

if (WCPG_FOUND AND NOT TARGET WCPG::WCPG)
    find_package(GMP REQUIRED)
    find_package(MPFR REQUIRED)
    find_package(MPFI REQUIRED)
    add_library(WCPG::WCPG UNKNOWN IMPORTED)
    set_target_properties(WCPG::WCPG PROPERTIES
        IMPORTED_LOCATION "${WCPG_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${PC_WCPG_FLAGS_OTHER}"
        INTERFACE_INCLUDE_DIRECTORIES "${WCPG_INCLUDE_DIR}"
    )
    target_link_libraries(WCPG::WCPG INTERFACE GMP::GMP MPFR::MPFR MPFI::MPFI)
    if(WCPG_LOCAL_BUILD)
        add_dependencies(WCPG::WCPG WCPG)
    endif()
endif()

mark_as_advanced(WCPG_INCLUDE_DIR WCPG_LIBRARY)
