cmake_minimum_required(VERSION 3.20)
project(zenrts VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ZENRTS_BUILD_TESTS "Build tests" ON)
option(ZENRTS_BUILD_BENCHMARKS "Build benchmarks" ON)
option(ZENRTS_STATIC_LINK "Statically link zenrts executable" ON)

include(cmake/Dependencies.cmake)

add_subdirectory(src)

if(ZENRTS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ZENRTS_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

include(GNUInstallDirs)
install(
    FILES ${PROJECT_SOURCE_DIR}/include/zenrts.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
    DIRECTORY ${PROJECT_SOURCE_DIR}/include/zenrts
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
