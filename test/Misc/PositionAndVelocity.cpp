#include "gtest/gtest.h"

#include "MiningOptimization/PositionAndVelocity.h"

TEST(PositionAndVelocity, TestParsing)
{
    PositionAndVelocity pos;
    auto result = PositionAndVelocity::tryParse("(x=2862 y=141 dx=921 dy=901 h=760 p=d6d715af)", pos);

    EXPECT_EQ(true, result);
    EXPECT_EQ(2862, pos.x);
    EXPECT_EQ(141, pos.y);
    EXPECT_EQ(921, pos.dx);
    EXPECT_EQ(901, pos.dy);
    EXPECT_EQ(760, pos.heading);
    EXPECT_EQ(3604420015, pos.previousPositionsHash);
}
