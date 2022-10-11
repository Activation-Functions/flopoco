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
  PATHS ${PC_WCPG_INCLUDE_DIRS}
  DOC "Path of wcpg.h, the include file for GNU WCPG library"
)

FIND_LIBRARY(WCPG_LIBRARY
  NAMES wcpg
  PATHS ${PC_WCPG_LIBRARY_DIRS}
  DOC "Directory of the WCPG library"
)

set(WCPG_VERSION ${PC_WCPG_VERSION})

if (NOT WCPG_INCLUDE_DIR AND NOT WCPG_LIBRARY AND WCPG_BUILD_NOTFOUND)
  include(FetchContent)
  FetchContent_Declare(
    WCPG
    GIT_REPOSITORY https://github.com/fixif/WCPG.git
    GIT_TAG b90253a4a6a650300454f5656a7e8410e0493175
  )
  FetchContent_MakeAvailable(WCPG)
  if(${wcpg_POPULATED} AND NOT EXISTS "${wcpg_BINARY_DIR}/lib/libwcpg.a")
    execute_process(
      COMMAND                bash autogen.sh
      WORKING_DIRECTORY      ${wcpg_SOURCE_DIR}
      COMMAND_ECHO           STDOUT
      RESULT_VARIABLE        WCPG_LOCAL_AUTOGEN_RES
    )
    if (${WCPG_LOCAL_AUTOGEN_RES} EQUAL "0")
      execute_process(
        COMMAND                ./configure --prefix=${wcpg_BINARY_DIR}
        WORKING_DIRECTORY      ${wcpg_SOURCE_DIR}
        COMMAND_ECHO           STDOUT
        RESULT_VARIABLE        WCPG_LOCAL_CONFIGURE_RES
      )
      if(${WCPG_LOCAL_CONFIGURE_RES} EQUAL 0)
        execute_process(
          COMMAND                make -j 4 install
          WORKING_DIRECTORY      ${wcpg_SOURCE_DIR}
          COMMAND_ECHO           STDOUT
          RESULT_VARIABLE        WCPG_LOCAL_INSTALL_RES
        )
        if(${WCPG_LOCAL_INSTALL_RES} EQUAL "0")
          set(WCPG_LIBRARY ${wcpg_BINARY_DIR}/lib/libwcpg.a)
          set(WCPG_INCLUDE_DIR ${wcpg_BINARY_DIR}/include)
          set(WCPG_LOCAL_BUILD)
        else()
          message(WARNING "Error when running local installation of WCPG")
        endif()
      else()
        message(WARNING "Error when configuring local WCPG")
      endif()
    else()
      message(WARNING "Error when running autogen for local build of WCPG")
    endif()
  endif()

  
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  WCPG
  FOUND_VAR WCPG_FOUND 
  REQUIRED_VARS
    WCPG_LIBRARY
    WCPG_INCLUDE_DIR
  VERSION_VAR WCPG_VERSION
)

if(WCPG_FOUND)
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
    INTERFACE_INCLUDE_DIRECTORRIES "${WCPG_INCLUDE_DIR}"
  )
  target_link_libraries(WCPG::WCPG INTERFACE GMP::GMP MPFR::MPFR MPFI::MPFI)
  if(WCPG_LOCAL_BUILD)
    add_dependencies(WCPG::WCPG WCPG)
  endif()
endif()

mark_as_advanced(WCPG_INCLUDE_DIR WCPG_LIBRARY)
