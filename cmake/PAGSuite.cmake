# Distributed under the FloPoCo License, see README.md for more information

#[=======================================================================[.rst:
PAGSuite
-------

Finds the PAGSuite libraries.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``PAGSuite::RPAG``
  The RPAG library

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
  PATHS ${PC_RPAG_INCLUDE_DIRS}
  DOC "Path of pagsuite/rpag.h, the include file for RPAG library"
)
FIND_LIBRARY(RPAG_LIBRARY
  NAMES rpag
  PATHS ${PC_RPAG_LIBRARY_DIRS}
  DOC "Directory of the RPAG library"
)
set(RPAG_VERSION ${PC_RPAG_VERSION})

# Handling PAG 
pkg_check_modules(PC_PAG QUIET pag)

find_path(PAG_INCLUDE_DIR
  NAMES pagsuite/adder_graph.h
  PATHS ${PC_PAG_INCLUDE_DIRS}
  DOC "Path of pagsuite/adder_graph.h, the include file for PAG library"
)
FIND_LIBRARY(PAG_LIBRARY
  NAMES pag
  PATHS ${PC_PAG_LIBRARY_DIRS}
  DOC "Directory of the PAG library"
)
set(PAG_VERSION ${PC_PAG_VERSION})

# Handling OSCM
pkg_check_modules(PC_OSCM QUIET oscm)

find_path(OSCM_INCLUDE_DIR
  NAMES pagsuite/oscm.h
  PATHS ${PC_OSCM_INCLUDE_DIRS}
  DOC "Path of pagsuite/oscm.h, the include file for OSCM library"
)
FIND_LIBRARY(OSCM_LIBRARY
  NAMES oscm
  PATHS ${PC_OSCM_LIBRARY_DIRS}
  DOC "Directory of the OSCM library"
)
set(OSCM_VERSION ${PC_OSCM_VERSION})

if(RPAG_LIBRARY AND RPAG_INCLUDE_DIR AND PAG_LIBRARY AND PAG_INCLUDE_DIR AND OSCM_LIBRARY AND OSCM_INCLUDE_DIR)
  set(PAGSUITE_ALL_FOUND)
endif()

# Build if not found and option is set 

if (NOT PAGSUITE_ALL_FOUND AND PAGSUITE_BUILD_NOTFOUND)
message(STATUS "PAGsuite not found, will attempt to build locally")
find_package(ScaLP)
  if(ScaLP_FOUND)
    set(ScaLP_DIR "${ScaLP_INCLUDE_DIR}/..")
    include(FetchContent)
    set(PAGSUITE_INSTALL_DIR ${CMAKE_BINARY_DIR}/PAGSuite-localbuild)
    FetchContent_Declare(
        PAGSuite
        GIT_REPOSITORY https://gitlab.com/kumm/pagsuite.git
        GIT_TAG 592d7ef9c105c804b7088b24903b097ee309da34
        SOURCE_SUBDIR DO_NOT_USE_CMAKE
    )
    set(SCALP_PREFIX_PATH ${ScaLP_DIR})
    FetchContent_MakeAvailable(PAGSuite)
    #set(RPAG_INCLUDE_DIR ${pagsuite_BINARY_DIR}/include)
    #set(RPAG_LIBRARY ${pagsuite_BINARY_DIR}/lib/rpag)
    #set(PAG_INCLUDE_DIR ${pagsuite_BINARY_DIR}/include)
    #set(PAG_LIBRARY ${pagsuite_BINARY_DIR}/lib/pag)
    #set(OSCM_INCLUDE_DIR ${pagsuite_BINARY_DIR}/include)
    #set(OSCM_LIBRARY ${pagsuite_BINARY_DIR}/lib/oscm)
    #set(PAGSUITE_LOCALLY_BUILT)
  else()
    message(STATUS "Missing ScaLP: PAGSuite will not be built locally")
  endif()
endif()

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

# RPAG

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

# PAG 

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

# OSCM 

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
