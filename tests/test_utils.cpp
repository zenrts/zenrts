#include <gtest/gtest.h>
#include <zenrts/utils.h>

TEST(UtilsTest, Greet)
{
    auto msg = zenrts::greet("World");
    EXPECT_EQ(msg, "Hello, World!");
}
