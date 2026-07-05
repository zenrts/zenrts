add_library(zenrts STATIC
    core.cpp
    utils.cpp
)

target_include_directories(zenrts
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_options(zenrts PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
        -Wall -Wextra -Wpedantic
    >
)

if(ZENRTS_BUILD_COVERAGE)
    target_compile_options(zenrts PRIVATE --coverage -g -O0)
    target_link_options(zenrts PUBLIC --coverage)
endif()

install(TARGETS zenrts
    EXPORT zenrts-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
