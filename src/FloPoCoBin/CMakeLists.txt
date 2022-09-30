# Detect GMPXX
find_package(GMPXX REQUIRED)

# Detect MPFR
find_package(MPFR REQUIRED)

add_executable(flopoco src/main)

target_link_libraries(flopoco FloPoCoLib)

add_executable(fp2bin src/fp2bin)
target_link_libraries(fp2bin MPFR::MPFR GMPXX::GMPXX FloPoCoLib)

add_executable(bin2fp src/bin2fp)
target_link_libraries(bin2fp MPFR::MPFR GMPXX::GMPXX FloPoCoLib)

add_executable(ieee2bin src/ieee2bin)
target_link_libraries(ieee2bin MPFR::MPFR GMPXX::GMPXX FloPoCoLib )


add_executable(bin2ieee src/bin2ieee)
target_link_libraries(bin2ieee MPFR::MPFR GMPXX::GMPXX FloPoCoLib)

add_executable(longacc2fp src/longacc2fp)
target_link_libraries(longacc2fp MPFR::MPFR GMPXX::GMPXX FloPoCoLib)