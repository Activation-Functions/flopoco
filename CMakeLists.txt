cmake_minimum_required(VERSION 3.11)
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
	set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

PROJECT(FloPoCo C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

MESSAGE("Trying to build FloPoCo on " ${CMAKE_SYSTEM_NAME} ", compiler is " ${CMAKE_C_COMPILER})
Message("CMAKE_CURRENT_BINARY_DIR is " ${CMAKE_CURRENT_BINARY_DIR}  ", CMAKE_CURRENT_SOURCE_DIR is " ${CMAKE_CURRENT_SOURCE_DIR})

cmake_policy(SET CMP0028 NEW) # Support for imported targets
cmake_policy(SET CMP0115 OLD) # Allow source file extension deduction 

# Add module to find custom dependencies
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")


#####################For packaging (source file distrib so far)#######
# Use  make package_source to build a release
# Currently it doesn't work at all

SET(CPACK_SOURCE_GENERATOR "TGZ")
SET(CPACK_PACKAGE_VERSION_MAJOR "0")
SET(CPACK_PACKAGE_VERSION_MINOR "9")
SET(CPACK_PACKAGE_VERSION_PATCH "0")

INCLUDE(CPack)

#Compilation flags
string(APPEND CMAKE_CXX_FLAGS_INIT "-Wall")
string(APPEND CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall")
string(APPEND CMAKE_CXX_FLAGS_MINSIZEREL_INIT "-Wall")
string(APPEND CMAKE_CXX_FLAGS_RELEASE_INIT "-Wall")
string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "-Wall")

# Default build type is Release
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to Release as none was specified.")
  set(CMAKE_BUILD_TYPE "Release" CACHE
    STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Detect GMP / GMPXX
find_package(GMPXX REQUIRED)

# Detect MPFR
find_package(MPFR REQUIRED)

# Detect MPFI
find_package(MPFI REQUIRED)

# find boost random
find_package(Boost REQUIRED COMPONENTS random)

# Detect Sollya
find_package(Sollya REQUIRED)

# Detect WCPG
OPTION(WCPG_BUILD_NOTFOUND "Attempt to build WCPG if not found" ON)
find_package(WCPG)

# dectect if scalp is available
OPTION(SCALP_BUILD_NOTFOUND "Attempt to build SCALP if not found" ON)
find_package(ScaLP)

#detect PAG suite
OPTION(PAGSUITE_BUILD_NOTFOUND "Attempt to build PAGsuite if not found" ON)
include(PAGSuite)

# Detect sed
FIND_PATH(SED
	sed
	DOC "sed command"
	)

IF (SED)
	MESSAGE( STATUS "Found sed: ${SED}" )
ELSE (SED)
	MESSAGE( FATAL_ERROR "Could not find sed" )
ENDIF(SED)

ADD_DEFINITIONS(-DHAVE_LNS)

find_package(FLEX)

# necessary to include generated files
#INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src/ ${GMP_H} ${GMPXX_H} ${MPFI_H} ${MPFR_H} ${SOLLYA_H})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src/ ${GMP_H} ${GMPXX_H} ${MPFI_H} ${MPFR_H} ${SOLLYA_H})

FLEX_TARGET(
	VHDLLexer ${CMAKE_CURRENT_SOURCE_DIR}/src/VHDLLexerRule.l
	${CMAKE_CURRENT_BINARY_DIR}/VHDLLexer.cpp
	COMPILE_FLAGS --nounput
)

#Thanks SOF https://stackoverflow.com/questions/4346412/how-to-prepend-all-filenames-on-the-list-with-common-path#27630120

# PREPEND(var prefix ${args}) set the variable var to the list [${prefix}x, for x in args]
FUNCTION(PREPEND var prefix)
  SET(listVar "")
  FOREACH(f ${ARGN})
	LIST(APPEND listVar "${prefix}${f}")
  ENDFOREACH(f)
  SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

execute_process(
  COMMAND ./tools/EraseComments.sh src/SourceFileList.txt tolink.txt
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/tolink.txt ACTIVE_THINGS)
PREPEND(PATH_ACTIVE_THINGS ${CMAKE_CURRENT_SOURCE_DIR}/src/ ${ACTIVE_THINGS})

execute_process(
  COMMAND rm tolink.txt
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

ADD_LIBRARY(FloPoCoLib STATIC ${PATH_ACTIVE_THINGS}
		${CMAKE_CURRENT_SOURCE_DIR}/src/Factories/Factories
		${CMAKE_CURRENT_BINARY_DIR}/VHDLLexer.cpp)

set_target_properties(FloPoCoLib PROPERTIES OUTPUT_NAME "FloPoCo")


TARGET_LINK_LIBRARIES(
  FloPoCoLib
  ${GMP_LIB} ${GMPXX_LIB} ${MPFI_LIB} ${MPFR_LIB} #xml2 ??xml2 not necessary??
  )

IF (Sollya_FOUND)
  TARGET_LINK_LIBRARIES(
	FloPoCoLib
	Sollya::Sollya
	)
ENDIF (Sollya_FOUND)

IF (WCPG_FOUND)
  TARGET_LINK_LIBRARIES(
	FloPoCoLib
	WCPG::WCPG
	)
ENDIF (WCPG_FOUND)

IF (ScaLP_FOUND)
	target_link_libraries(
			FloPoCoLib
			ScaLP::ScaLP
	)
ENDIF (ScaLP_FOUND)

IF (RPAG_FOUND)
	TARGET_LINK_LIBRARIES(
			FloPoCoLib
      PAGSuite::RPAG
	)
ENDIF (RPAG_FOUND)

IF (PAG_FOUND)
	TARGET_LINK_LIBRARIES(
			FloPoCoLib
			PAGSuite::PAG
	)
ENDIF (PAG_FOUND)

IF (OSCM_FOUND)
	TARGET_LINK_LIBRARIES(
			FloPoCoLib
			PAGSuite::OSCM
	)
ENDIF (OSCM_FOUND)

#IF (PAGSUITE_FOUND)
#    TARGET_LINK_LIBRARIES(
#            FloPoCoLib
#            ${PAGSUITE_LIBS}
#    )
#    INCLUDE_DIRECTORIES(${PAGSUITE_H})
#ENDIF (PAGSUITE_FOUND)



ADD_EXECUTABLE(flopoco
  src/main
  )

TARGET_LINK_LIBRARIES(
  flopoco
  FloPoCoLib
  )


ADD_EXECUTABLE(fp2bin src/Tools/fp2bin  src/utils)
TARGET_LINK_LIBRARIES(fp2bin ${MPFR_LIB} ${GMP_LIB} ${GMPXX_LIB} FloPoCoLib)

ADD_EXECUTABLE(bin2fp src/Tools/bin2fp  src/utils)
TARGET_LINK_LIBRARIES(bin2fp ${MPFR_LIB} ${GMP_LIB} ${GMPXX_LIB} FloPoCoLib)

ADD_EXECUTABLE(ieee2bin src/Tools/ieee2bin  src/utils)
TARGET_LINK_LIBRARIES(ieee2bin ${MPFR_LIB} ${GMP_LIB} ${GMPXX_LIB} FloPoCoLib )


ADD_EXECUTABLE(bin2ieee src/Tools/bin2ieee  src/utils)
TARGET_LINK_LIBRARIES(bin2ieee ${MPFR_LIB} ${GMP_LIB} ${GMPXX_LIB} FloPoCoLib)

ADD_EXECUTABLE(longacc2fp src/Tools/longacc2fp  src/utils)
TARGET_LINK_LIBRARIES(longacc2fp ${MPFR_LIB} ${GMP_LIB} ${GMPXX_LIB} FloPoCoLib)

# To resurrect some day if we finish the work on FFT
# ADD_EXECUTABLE(fftest src/Complex/fakemain)
# TARGET_LINK_LIBRARIES(fftest ${MPFR_LIB} ${GMP_LIB} ${GMPXX_LIB} FloPoCoLib)

#add_subdirectory(src/random)
#target_link_libraries(FloPoCoLib flopoco_random_lib)

OPTION(BUILD_UNIT_TEST "Build the flopoco unit tests")
if(BUILD_UNIT_TEST)
	find_package(Boost 1.55 REQUIRED COMPONENTS unit_test_framework)
	enable_testing()
	## Testing Table class
	add_executable(TableTest_exe tests/Table/Table.cpp)
	target_link_libraries(TableTest_exe FloPoCoLib ${Boost_LIBRARIES})
	add_test(TableTest TableTest_exe)

	## Testing Posit format
	add_executable(NumberFormatTest_exe tests/TestBenches/PositNumber.cpp)
	target_include_directories(NumberFormatTest_exe PUBLIC ${Boost_INCLUDE_DIR})
	target_link_libraries(NumberFormatTest_exe FloPoCoLib ${Boost_LIBRARIES})

	find_library(SOFTPOSIT_LIB softposit)
	find_path(SOFTPOSIT_H softposit.h)

	if(SOFTPOSIT_LIB AND SOFTPOSIT_H)
		MESSAGE(STATUS "softposit found : ${SOFTPOSIT_LIB}, ${SOFTPOSIT_H}")
		OPTION(POSIT32_TEST "Exhaustive test of conversion from and back posit32
		(long)")
		target_compile_definitions(NumberFormatTest_exe PRIVATE SOFTPOSIT)
		if(POSIT32_TEST)
			target_compile_definitions(NumberFormatTest_exe PRIVATE POSIT32TEST)
		endif()
		target_link_libraries(NumberFormatTest_exe ${SOFTPOSIT_LIB})
		target_include_directories(NumberFormatTest_exe PRIVATE ${SOFTPOSIT_H})
	else()
		Message(WARNING "Softposit not found, will not check if mpfr conversion from value is correct")
	endif()

	add_test(NumberFormatTest NumberFormatTest_exe)
	## Testing IntConstMultShiftAdd adder cost computation


	add_executable(ErrorGraphTest_exe tests/ConstMult/testErrorGraphCost.cpp ${CMAKE_CURRENT_BINARY_DIR}/Factories.cpp ${CMAKE_CURRENT_BINARY_DIR}/VHDLLexer.cpp	)
	target_include_directories(ErrorGraphTest_exe PUBLIC ${Boost_INCLUDE_DIR} ${PAGSUITE_INCLUDE_DIR})
	target_link_libraries(ErrorGraphTest_exe FloPoCoLib ${Boost_LIBRARIES} ${RPAG_LIB})

	if(PAGLIB_FOUND AND RPAG_FOUND)
		add_test(ErrorGraphCost ErrorGraphTest_exe)

		add_executable(IntConstMultShiftAddCostFunction_exe tests/ConstMult/testNodeCost.cpp ${CMAKE_CURRENT_BINARY_DIR}/Factories.cpp ${CMAKE_CURRENT_BINARY_DIR}/VHDLLexer.cpp	)
		target_include_directories(IntConstMultShiftAddCostFunction_exe PUBLIC ${Boost_INCLUDE_DIR} ${PAGSUITE_INCLUDE_DIR})
		target_link_libraries(IntConstMultShiftAddCostFunction_exe FloPoCoLib ${Boost_LIBRARIES} ${RPAG_LIB})

		add_test(IntConstMultShiftAddCost IntConstMultShiftAddCostFunction_exe)
	endif()
endif()
