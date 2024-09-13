# Detect GMP / GMPXX
find_package(GMPXX REQUIRED)

add_library(hwtargets SHARED)

target_include_directories(hwtargets PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> 
   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

function(add_hwtargets_src)
  target_sources(hwtargets PRIVATE ${ARGN} )
endfunction(add_hwtargets_src)

add_subdirectory(src)

install(TARGETS hwtargets EXPORT hwtargets-config
        LIBRARY 
          DESTINATION ${CMAKE_INSTALL_LIBDIR}
          COMPONENT hwtargets
  )

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/flopoco DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
  EXPORT hwtargets-config
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
