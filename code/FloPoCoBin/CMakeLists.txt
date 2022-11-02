# Detect GMPXX
find_package(GMPXX REQUIRED)

# Detect MPFR
find_package(MPFR REQUIRED)

add_executable(flopoco src/main.cpp)

target_link_libraries(flopoco FloPoCoLib)

add_executable(fp2bin src/fp2bin.cpp)
target_link_libraries(fp2bin MPFR::MPFR GMPXX::GMPXX FloPoCoLib)

add_executable(bin2fp src/bin2fp.cpp)
target_link_libraries(bin2fp MPFR::MPFR GMPXX::GMPXX FloPoCoLib)

add_executable(ieee2bin src/ieee2bin.cpp)
target_link_libraries(ieee2bin MPFR::MPFR GMPXX::GMPXX FloPoCoLib )

add_executable(minimal_main src/main_minimal.cpp)
target_link_libraries(minimal_main GMPXX::GMPXX FloPoCoLib)

add_executable(bin2ieee src/bin2ieee.cpp)
target_link_libraries(bin2ieee MPFR::MPFR GMPXX::GMPXX FloPoCoLib)

add_executable(longacc2fp src/longacc2fp.cpp)
target_link_libraries(longacc2fp MPFR::MPFR GMPXX::GMPXX FloPoCoLib)