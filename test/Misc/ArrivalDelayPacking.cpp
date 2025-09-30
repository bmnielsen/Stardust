#include "BWTest.h"
#include "DoNothingModule.h"

namespace
{
    int8_t packArrivalDelay(int arrivalDelay, bool facingPatch)
    {
        int8_t result = (int8_t)arrivalDelay * 2;
        if (!facingPatch)
        {
            result |= 0b00000001;
        }
        return result;
    }

    std::pair<int, bool> unpackArrivalDelay(int8_t packedArrivalDelay)
    {
        return std::make_pair(packedArrivalDelay >> 1, !(packedArrivalDelay & 1));
    }
}

TEST(ArrivalDelayPacking, TestCases)
{
    auto testCase = [](int arrivalDelay, bool facingPatch)
    {
        auto packed = packArrivalDelay(arrivalDelay, facingPatch);
        auto unpacked = unpackArrivalDelay(packed);
        EXPECT_EQ(arrivalDelay, unpacked.first);
        EXPECT_EQ(facingPatch, unpacked.second);
    };
    for (int i = -64; i < 64; i++)
    {
        testCase(i, false);
        testCase(i, true);
    }
}
