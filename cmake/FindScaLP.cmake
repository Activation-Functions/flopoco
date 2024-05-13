# Distributed under the FloPoCo License, see README.md for more information

#[=======================================================================[.rst:
FindScaLP
-------

Finds the ScaLP library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``ScaLP::ScaLP``
  The ScaLP library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``ScaLP_FOUND``
  True if the system has the ScaLP library.
``ScaLP_VERSION``
  The version of the ScaLP library which was found.
``ScaLP_INCLUDE_DIRS``
  Include directories needed to use ScaLP.
``ScaLP_LIBRARIES``
  Libraries needed to link to ScaLP.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``ScaLP_INCLUDE_DIR``
  The directory containing ``ScaLP/Solver.h``.
``ScaLP_LIBRARY``
  The path to the ScaLP library.

#]=======================================================================]

find_package(PkgConfig)

# -----------------------------------------------------------------------------
# Look for ScaLP system installation
pkg_check_modules(PC_ScaLP QUIET scalp)
# -----------------------------------------------------------------------------

find_path(ScaLP_INCLUDE_DIR
    NAMES ScaLP/Solver.h
    PATHS ${PC_ScaLP_INCLUDE_DIRS} ${SCALP_LOCAL}/include
    DOC "Path of ScaLP/Solver.h, the include file for ScaLP library"
)

find_library(ScaLP_LIBRARY
    NAMES libScaLP.dylib libScaLP libScaLP.so libScaLP.a
    PATHS ${PC_ScaLP_LIBRARY_DIRS} ${SCALP_LOCAL}/lib
    DOC "Directory of the ScaLP library"
)

set(ScaLP_VERSION ${PC_ScaLP_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    ScaLP
    FOUND_VAR ScaLP_FOUND
    REQUIRED_VARS
        ScaLP_LIBRARY
        ScaLP_INCLUDE_DIR
    VERSION_VAR ScaLP_VERSION
)

if (ScaLP_FOUND)
    set(ScaLP_LIBRARIES ${ScaLP_LIBRARY})
    set(ScaLP_INCLUDE_DIRS ${ScaLP_INCLUDE_DIR})
    set(ScaLP_DEFINITIONS ${PC_ScaLP_FLAGS_OTHER})
    if (NOT TARGET ScaLP::ScaLP)
        add_library(ScaLP::ScaLP UNKNOWN IMPORTED)
        set_target_properties(ScaLP::ScaLP PROPERTIES
            IMPORTED_LOCATION "${ScaLP_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${PC_ScaLP_FLAGS_OTHER}"
            INTERFACE_INCLUDE_DIRECTORIES "${ScaLP_INCLUDE_DIR}"
        )
        target_compile_definitions(ScaLP::ScaLP INTERFACE HAVE_SCALP)
    endif()
endif()

mark_as_advanced(ScaLP_INCLUDE_DIR ScaLP_LIBRARY)
