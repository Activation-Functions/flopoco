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

# Detect LAPACK
find_package(LAPACK REQUIRED)

# find boost random
find_package(Boost 1.64.0 REQUIRED COMPONENTS random filesystem)

# Detect Sollya
find_package(Sollya)

# Detect WCPG
find_package(WCPG)

# dectect if scalp is available
find_package(ScaLP)

#detect PAG suite
include(PAGSuite)

add_library(FloPoCoLib SHARED ${CMAKE_CURRENT_BINARY_DIR}/VHDLLexer.cpp)

target_include_directories(FloPoCoLib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(FloPoCoLib PROPERTIES OUTPUT_NAME "FloPoCo")

target_link_libraries(FloPoCoLib PUBLIC hileco)

target_link_libraries(FloPoCoLib PUBLIC hwtargets)

target_link_libraries(
    FloPoCoLib PRIVATE
    GMPXX::GMPXX MPFR::MPFR MPFI::MPFI LAPACK::LAPACK ${Boost_LIBRARIES}
)

if (Sollya_FOUND)
    target_link_libraries(FloPoCoLib PRIVATE Sollya::Sollya)
endif (Sollya_FOUND)

if (WCPG_FOUND)
    target_link_libraries(FloPoCoLib PRIVATE WCPG::WCPG)
    target_compile_definitions(FloPoCoLib PUBLIC HAVE_WCPG)
endif (WCPG_FOUND)

if (ScaLP_FOUND)
    target_link_libraries(FloPoCoLib PRIVATE ScaLP::ScaLP ${ScaLP_BACKENDS})
    target_compile_definitions(FloPoCoLib PUBLIC HAVE_SCALP)
endif (ScaLP_FOUND)

if (RPAG_FOUND)
    target_link_libraries(FloPoCoLib PRIVATE PAGSuite::RPAG)
    target_compile_definitions(FloPoCoLib PUBLIC HAVE_RPAGLIB)
endif (RPAG_FOUND)

if (PAG_FOUND)
    target_link_libraries(FloPoCoLib PRIVATE PAGSuite::PAG)
    target_compile_definitions(FloPoCoLib PUBLIC HAVE_PAGLIB)
endif (PAG_FOUND)

if (OSCM_FOUND)
    target_link_libraries(FloPoCoLib PRIVATE PAGSuite::OSCM)
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
  )

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/flopoco
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT flopocolib-config
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
