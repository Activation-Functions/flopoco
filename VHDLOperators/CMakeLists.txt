find_package(FLEX)
FLEX_TARGET(
	VHDLLexer ${CMAKE_CURRENT_SOURCE_DIR}/src/VHDLLexerRule.l
	${CMAKE_CURRENT_BINARY_DIR}/VHDLLexer.cpp
	COMPILE_FLAGS --nounput
)

# Detect GMP / GMPXX
find_package(GMPXX REQUIRED)

# Detect MPFR
find_package(MPFR REQUIRED)

# Detect MPFI
find_package(MPFI REQUIRED)

# find boost random
find_package(Boost REQUIRED COMPONENTS random)

# Detect Sollya
find_package(Sollya)

# Detect WCPG
find_package(WCPG)

# dectect if scalp is available
find_package(ScaLP)

#detect PAG suite
include(PAGSuite)

ADD_LIBRARY(FloPoCoLib STATIC ${PATH_ACTIVE_THINGS}
${CMAKE_CURRENT_BINARY_DIR}/VHDLLexer.cpp
)

target_include_directories(FloPoCoLib PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# TODO ugly workaround as of now, but will be problematic to ship the vhdl part of flopoco as a library
#target_include_directories(FloPoCoLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/flopoco)
set_target_properties(FloPoCoLib PROPERTIES OUTPUT_NAME "FloPoCo")

target_link_libraries(FloPoCoLib PUBLIC hileco)

target_link_libraries(
  FloPoCoLib PRIVATE
  GMPXX::GMPXX MPFR::MPFR MPFI::MPFI
  )

if (Sollya_FOUND)
  target_link_libraries(
	FloPoCoLib PRIVATE
	Sollya::Sollya
	)
endif (Sollya_FOUND)

if (WCPG_FOUND)
  target_link_libraries(
	FloPoCoLib PRIVATE
	WCPG::WCPG
	)
endif (WCPG_FOUND)

if (ScaLP_FOUND)
	target_link_libraries(
		FloPoCoLib PRIVATE 
		ScaLP::ScaLP
	)
	target_compile_definitions(FloPoCoLib PUBLIC HAVE_SCALP)
endif (ScaLP_FOUND)

if (RPAG_FOUND)
	target_link_libraries(
			FloPoCoLib
      PAGSuite::RPAG
	)
	target_compile_definitions(FloPoCoLib PUBLIC HAVE_RPAGLIB)
endif (RPAG_FOUND)

if (PAG_FOUND)
	target_link_libraries(
			FloPoCoLib
			PAGSuite::PAG
	)
	target_compile_definitions(FloPoCoLib PUBLIC HAVE_PAGLIB)
endif (PAG_FOUND)

if (OSCM_FOUND)
	target_link_libraries(
			FloPoCoLib
			PAGSuite::OSCM
	)
	target_compile_definitions(FloPoCoLib PUBLIC HAVE_OSCM)
endif (OSCM_FOUND)

function(add_flopocolib_src)
  target_sources(FloPoCoLib PRIVATE ${ARGN} )
endfunction(add_flopocolib_src)

set(flopocolib_header_root ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_subdirectory(src)


install(TARGETS FloPoCoLib EXPORT flopocolib-config
        LIBRARY 
          DESTINATION ${CMAKE_INSTALL_LIBDIR}
          COMPONENT flopocolib
        PUBLIC_HEADER
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          COMPONENT flopocolib 
  )

install(
    EXPORT flopocolib-config
	NAMESPACE ${PROJECT_NAME}::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})