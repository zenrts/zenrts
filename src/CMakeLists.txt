add_library(zenrts_lib STATIC
    core.cpp
    utils.cpp
)

target_include_directories(zenrts_lib
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_options(zenrts_lib PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
        -Wall -Wextra -Wpedantic
    >
)

add_executable(zenrts main.cpp)

if(ZENRTS_BUILD_COVERAGE)
    target_compile_options(zenrts_lib PRIVATE --coverage -g -O0)
    target_link_options(zenrts_lib PUBLIC --coverage)
    target_compile_options(zenrts PRIVATE --coverage -g -O0)
    target_link_options(zenrts PRIVATE --coverage)
endif()
target_link_libraries(zenrts PRIVATE zenrts_lib)

if(ZENRTS_STATIC_LINK)
    target_link_options(zenrts PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-static -static-libstdc++ -static-libgcc>
        $<$<AND:$<CXX_COMPILER_ID:Clang>,$<NOT:$<PLATFORM_ID:Darwin>>>:-static -static-libstdc++ -static-libgcc>
        $<$<AND:$<CXX_COMPILER_ID:Clang>,$<PLATFORM_ID:Darwin>>:-static-libstdc++>
    )
endif()

install(TARGETS zenrts RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
