add_executable(test_zenrts
    test_core.cpp
    test_utils.cpp
)

target_link_libraries(test_zenrts PRIVATE zenrts gtest_main)

add_test(NAME test_zenrts COMMAND test_zenrts)
