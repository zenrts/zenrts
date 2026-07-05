add_library(zenrts
    core.cpp
    utils.cpp
)

target_include_directories(zenrts
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_options(zenrts PRIVATE -Wall -Wextra -Wpedantic)
