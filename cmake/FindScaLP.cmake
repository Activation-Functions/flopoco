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
pkg_check_modules(PC_ScaLP QUIET scalp)

find_path(ScaLP_INCLUDE_DIR
    NAMES ScaLP/Solver.h
    PATHS ${PC_ScaLP_INCLUDE_DIRS}
    DOC "Path of ScaLP/Solver.h, the include file for ScaLP library"
)

find_library(ScaLP_LIBRARY
    NAMES libScaLP.dylib libScaLP libScaLP.so
    PATHS ${PC_ScaLP_LIBRARY_DIRS}
    DOC "Directory of the ScaLP library"
)

set(ScaLP_VERSION ${PC_ScaLP_VERSION})

if(ScaLP_INCLUDE_DIR AND ScaLP_LIBRARY)
  find_library(SCALP_CPLEX_LIB
      NAMES libScaLP-CPLEX libScaLP-CPLEX.so libScaLP-CPLEX.dylib
      HINTS "/usr/local/lib64/" "/usr/local/lib/" "${SCALP_PREFIX_DIR}/" "${SCALP_PREFIX_DIR}/lib/" "$ENV{SCALP_PREFIX_DIR}/lib/"  
      DOC "Directory of the SCALP library for cplex solver backend"
  )
  LIST(APPEND ScaLP_BACKENDS ${SCALP_CPLEX_LIB})

  find_library(SCALP_GUROBI_LIB
      NAMES libScaLP-Gurobi libScaLP-Gurobi.so libScaLP-Gurobi.dylib
      HINTS "/usr/local/lib64/" "/usr/local/lib/" "${SCALP_PREFIX_DIR}/" "${SCALP_PREFIX_DIR}/lib/" "$ENV{SCALP_PREFIX_DIR}/lib/"
      DOC "Directory of the SCALP library for Gurobi solver backend"
  )
  LIST(APPEND ScaLP_BACKENDS ${SCALP_GUROBI_LIB})

  find_library(SCALP_SCIP_LIB
      NAMES libScaLP-SCIP libScaLP-SCIP.so libScaLP-SCIP.dylib
      HINTS "/usr/local/lib64/" "/usr/local/lib/" "${SCALP_PREFIX_DIR}/" "${SCALP_PREFIX_DIR}/lib/" "$ENV{SCALP_PREFIX_DIR}/lib/"
      DOC "Directory of the SCALP library for SCIP solver backend"
  )
  LIST(APPEND ScaLP_BACKENDS ${SCALP_SCIP_LIB})

  find_library(SCALP_LPSOLVE_LIB
      NAMES libScaLP-LPSOLVE libScaLP-LPSOLVE.so libScaLP-LPSolve.so libScaLP-LPSOLVE.dylib
      HINTS "/usr/local/lib64/" "/usr/local/lib/" "${SCALP_PREFIX_DIR}/" "${SCALP_PREFIX_DIR}/lib/" "$ENV{SCALP_PREFIX_DIR}/lib/"
      DOC "Directory of the SCALP library for LPSOLVE solver backend"
  )
  LIST(APPEND ScaLP_BACKENDS ${SCALP_LPSOLVE_LIB})
elseif (SCALP_BUILD_NOTFOUND)
  message(STATUS "ScaLP not found, will attempt to build")
  find_library(LPSOLVE_LIB 
      NAMES lpsolve55
      PATH_SUFFIXES lpsolve lp_solve)
  message("lpsolve found: ${LPSOLVE_LIB}")
  find_path(LPSOLVE_INCLUDE_DIR NAMES lpsolve/lp_types.h)
  if (LPSOLVE_LIB AND LPSOLVE_INCLUDE_DIR)
      get_filename_component(LPSOLVE_LIB_DIR ${LPSOLVE_LIB} DIRECTORY)
      include(FetchContent)
      FetchContent_Declare(
          Scalp
          GIT_REPOSITORY https://digidev.digi.e-technik.uni-kassel.de/git/scalp.git
          SOURCE_SUBDIR NonExistentRepo
        )
      FetchContent_MakeAvailable(Scalp)
      set(SCALP_REAL_SOURCE_DIR ${scalp_SOURCE_DIR})
      if(${scalp_POPULATED} AND NOT EXISTS "${scalp_BINARY_DIR}/include/ScaLP/SolverDynamic.h")
        file(REMOVE_RECURSE ${SCALP_REAL_SOURCE_DIR}/build)
        file(MAKE_DIRECTORY ${SCALP_REAL_SOURCE_DIR}/build)
        execute_process(
          COMMAND           cmake -B build -G${USED_CMAKE_GENERATOR} -DCMAKE_INSTALL_PREFIX=${scalp_BINARY_DIR} -DUSE_LPSOLVE=ON -DLPSOLVE_LIBRARIES=${LPSOLVE_LIB_DIR} -DLPSOLVE_INCLUDE_DIRS=${LPSOLVE_INCLUDE_DIR} -DBUILD_SHARED_LIBRARIES=OFF.
          WORKING_DIRECTORY ${SCALP_REAL_SOURCE_DIR}
          COMMAND_ECHO      STDOUT
        )
        execute_process(
          COMMAND           cmake --build build --target install 
          WORKING_DIRECTORY ${SCALP_REAL_SOURCE_DIR}
          COMMAND_ECHO      STDOUT
        )
      endif()

      set(ScaLP_INCLUDE_DIR ${scalp_BINARY_DIR}/include)
      set(ScaLP_LIBRARY ${scalp_BINARY_DIR}/lib/libScaLP.so)
      set(SCALP_LPSOLVE_LIB ${scalp_BINARY_DIR}/lib/libScaLP-LPSOLVE.so)
      set(ScaLP_BACKENDS ${SCALP_LPSOLVE_LIB})
      set(SCALP_LOCALLY_BUILT)
  else()
      message(STATUS "lpsolve not found, ScaLP cannot be built locally")
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
ScaLP
FOUND_VAR ScaLP_FOUND 
REQUIRED_VARS
    ScaLP_LIBRARY
    ScaLP_INCLUDE_DIR
VERSION_VAR ScaLP_VERSION
)

if(ScaLP_FOUND)
 set(ScaLP_LIBRARIES ${ScaLP_LIBRARY})
 set(ScaLP_INCLUDE_DIRS ${ScaLP_INCLUDE_DIR})
 set(ScaLP_DEFINITIONS ${PC_ScaLP_FLAGS_OTHER})
endif()

if (ScaLP_FOUND AND NOT TARGET ScaLP::ScaLP)
  add_library(ScaLP::ScaLP UNKNOWN IMPORTED)
  set_target_properties(ScaLP::ScaLP PROPERTIES 
      IMPORTED_LOCATION "${ScaLP_LIBRARY}"
      INTERFACE_COMPILE_OPTIONS "${PC_ScaLP_FLAGS_OTHER}"
      INTERFACE_INCLUDE_DIRECTORIES "${ScaLP_INCLUDE_DIR}"
  )
  #if (SCALP_LOCALLY_BUILT)
  #  add_dependencies(ScaLP::ScaLP PUBLIC Scalp)
  #endif()
endif()
if(TARGET ScaLP::ScaLP)
  target_compile_definitions(ScaLP::ScaLP INTERFACE HAVE_SCALP)
endif()
mark_as_advanced(ScaLP_INCLUDE_DIR ScaLP_LIBRARY)
