#include <gtest/gtest.h>
#include <zenrts/core.h>

TEST(CoreTest, Version)
{
    auto v = zenrts::version();
    EXPECT_EQ(v, "0.1.0");
}
