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
cmake_policy(SET CMP0076 NEW) # Conversion of target_sources argument to absolute
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
string(APPEND CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall")
string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "-Wall")

# Default build type is Release
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to Release as none was specified.")
  set(CMAKE_BUILD_TYPE "Release" CACHE
    STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()


OPTION(WCPG_BUILD_NOTFOUND "Attempt to build WCPG if not found" ON)
OPTION(SCALP_BUILD_NOTFOUND "Attempt to build SCALP if not found" ON)
OPTION(PAGSUITE_BUILD_NOTFOUND "Attempt to build PAGsuite if not found" ON)

add_subdirectory(HighLevelCores)
add_subdirectory(VHDLOperators)
add_subdirectory(FloPoCoBin)


# To resurrect some day if we finish the work on FFT
# ADD_EXECUTABLE(fftest src/Complex/fakemain)
# TARGET_LINK_LIBRARIES(fftest ${MPFR_LIB} ${GMP_LIB} ${GMPXX_LIB} FloPoCoLib)

#add_subdirectory(random)

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
