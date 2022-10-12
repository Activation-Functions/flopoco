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