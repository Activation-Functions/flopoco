# Detect GMP / GMPXX
find_package(GMPXX REQUIRED)

# Detect Sollya
find_package(Sollya)

add_library(hileco SHARED)
target_include_directories(hileco PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(hileco PUBLIC GMPXX::GMPXX)

if (Sollya_FOUND)
  target_link_libraries(hileco PUBLIC Sollya::Sollya)
endif()

function(add_hileco_src)
  target_sources(hileco PRIVATE ${ARGN} )
endfunction(add_hileco_src)

add_subdirectory(src)

install(TARGETS hileco EXPORT hileco-config
        LIBRARY 
          DESTINATION ${CMAKE_INSTALL_LIBDIR}
          COMPONENT hileco
        PUBLIC_HEADER
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          COMPONENT hileco 
  )

install(
  EXPORT hileco-config
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
