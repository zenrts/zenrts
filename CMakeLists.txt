# ZenRTS
# Copyright (C) 2026  Ian Torres
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License version 3
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.20)
project(zenrts VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ZENRTS_BUILD_TESTS "Build tests" ON)
option(ZENRTS_BUILD_BENCHMARKS "Build benchmarks" ON)
option(ZENRTS_BUILD_COVERAGE "Build with code coverage" OFF)

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
install(
    EXPORT zenrts-targets
    FILE zenrtsConfig.cmake
    NAMESPACE zenrts::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zenrts
)
