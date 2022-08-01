# Detect GMP / GMPXX
find_package(GMPXX REQUIRED)

# Detect Sollya
find_package(Sollya)

add_library(hileco SHARED)
target_include_directories(hileco PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

function(add_hileco_src)
  target_sources(hileco PRIVATE ${ARGN} )
endfunction(add_hileco_src)

add_subdirectory(src)
