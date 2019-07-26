#include "BWTest.h"

#include "LocutusAIModule.h"

TEST(SelfPlay, ATest)
{
    BWTest test;
    test.opponentModule = new LocutusAIModule();
    test.run();

    EXPECT_EQ(1, 1);
}
