# Distributed under the FloPoCo License, see README.md for more information

#[=======================================================================[.rst:
PAGSuite
-------

Finds the PAGSuite libraries.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``PAGSuite::RPAG``
  The RPAG binary

``PAGSuite::PAG``
  The PAG library

``PAGSuite::OSCM``
  The OSCM library

#]=======================================================================]

find_package(PkgConfig)

include(FindPackageHandleStandardArgs)

# Handling RPAG
pkg_check_modules(PC_RPAG QUIET rpag)

find_path(RPAG_INCLUDE_DIR
    NAMES pagsuite/rpag.h
    PATHS ${PC_RPAG_INCLUDE_DIRS} ${PAG_LOCAL}/include
    DOC "Path of pagsuite/rpag.h, the include file for RPAG library"
)

find_library(RPAG_LIBRARY
    NAMES rpag
    PATHS ${PC_RPAG_LIBRARY_DIRS} ${PAG_LOCAL}/lib
    DOC "Directory of the RPAG library"
)
set(RPAG_VERSION ${PC_RPAG_VERSION})

# Handling PAG
pkg_check_modules(PC_PAG QUIET pag)

find_path(PAG_INCLUDE_DIR
    NAMES pagsuite/adder_graph.h
    PATHS ${PC_PAG_INCLUDE_DIRS} ${PAG_LOCAL}/include
    DOC "Path of pagsuite/adder_graph.h, the include file for PAG library"
)
find_library(PAG_LIBRARY
    NAMES pag
    PATHS ${PC_PAG_LIBRARY_DIRS} ${PAG_LOCAL}/lib
    DOC "Directory of the PAG library"
)
set(PAG_VERSION ${PC_PAG_VERSION})

# Handling OSCM
pkg_check_modules(PC_OSCM QUIET oscm)

find_path(OSCM_INCLUDE_DIR
  NAMES pagsuite/oscm.hpp
  PATHS ${PC_OSCM_INCLUDE_DIRS} ${PAG_LOCAL}/include
  DOC "Path of pagsuite/oscm.h, the include file for OSCM library"
)

find_library(OSCM_LIBRARY
    NAMES oscm
    PATHS ${PC_OSCM_LIBRARY_DIRS} ${PAG_LOCAL}/lib
    DOC "Directory of the OSCM library"
)
set(OSCM_VERSION ${PC_OSCM_VERSION})

find_package_handle_standard_args(
    PAG
    FOUND_VAR PAG_FOUND
    REQUIRED_VARS
        PAG_LIBRARY
        PAG_INCLUDE_DIR
    VERSION_VAR PAG_VERSION
)

find_package_handle_standard_args(
    RPAG
    FOUND_VAR RPAG_FOUND
    REQUIRED_VARS
        RPAG_LIBRARY
        RPAG_INCLUDE_DIR
    VERSION_VAR RPAG_VERSION
)

find_package_handle_standard_args(
    OSCM
    FOUND_VAR OSCM_FOUND
    REQUIRED_VARS
        OSCM_LIBRARY
        OSCM_INCLUDE_DIR
    VERSION_VAR OSCM_VERSION
)

if(RPAG_FOUND)
    set(RPAG_LIBRARIES ${RPAG_LIBRARY})
    set(RPAG_INCLUDE_DIRS ${RPAG_INCLUDE_DIR})
    set(RPAG_DEFINITIONS ${PC_RPAG_FLAGS_OTHER})
endif()

if (RPAG_FOUND AND NOT TARGET PAGSuite::RPAG)
    add_library(PAGSuite::RPAG UNKNOWN IMPORTED)
    set_target_properties(PAGSuite::RPAG PROPERTIES
        IMPORTED_LOCATION "${RPAG_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${PC_RPAG_FLAGS_OTHER}"
        INTERFACE_INCLUDE_DIRECTORIES "${RPAG_INCLUDE_DIR}"
    )
    if (PAGSUITE_LOCALLY_BUILT)
       add_dependencies(PAGSuite::RPAG PAGSuite)
    endif()
endif()
mark_as_advanced(RPAG_INCLUDE_DIR RPAG_LIBRARY)

if(PAG_FOUND)
    set(PAG_LIBRARIES ${PAG_LIBRARY})
    set(PAG_INCLUDE_DIRS ${PAG_INCLUDE_DIR})
    set(PAG_DEFINITIONS ${PC_PAG_FLAGS_OTHER})
endif()

if (PAG_FOUND AND NOT TARGET PAGSuite::PAG)
    add_library(PAGSuite::PAG UNKNOWN IMPORTED)
    set_target_properties(PAGSuite::PAG PROPERTIES
        IMPORTED_LOCATION "${PAG_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${PC_PAG_FLAGS_OTHER}"
        INTERFACE_INCLUDE_DIRECTORIES "${PAG_INCLUDE_DIR}"
    )
    target_compile_definitions(PAGSuite::PAG INTERFACE HAVE_PAGLIB)
    if (PAGSUITE_LOCALLY_BUILT)
       add_dependencies(PAGSuite::PAG PAGSuite)
    endif()
endif()
mark_as_advanced(PAG_INCLUDE_DIR PAG_LIBRARY)

if(OSCM_FOUND)
    set(OSCM_LIBRARIES ${OSCM_LIBRARY})
    set(OSCM_INCLUDE_DIRS ${OSCM_INCLUDE_DIR})
    set(OSCM_DEFINITIONS ${PC_OSCM_FLAGS_OTHER})
endif()

if (OSCM_FOUND AND NOT TARGET PAGSuite::OSCM)
    add_library(PAGSuite::OSCM UNKNOWN IMPORTED)
    set_target_properties(PAGSuite::OSCM PROPERTIES
        IMPORTED_LOCATION "${OSCM_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${PC_OSCM_FLAGS_OTHER}"
        INTERFACE_INCLUDE_DIRECTORIES "${OSCM_INCLUDE_DIR}"
    )
    target_compile_definitions(PAGSuite::OSCM INTERFACE HAVE_OSCM)
    if (PAGSUITE_LOCALLY_BUILT)
       add_dependencies(PAGSuite::OSCM PAGSuite)
    endif()
endif()
mark_as_advanced(OSCM_INCLUDE_DIR OSCM_LIBRARY)
