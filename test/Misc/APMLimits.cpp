#include "BWTest.h"

#include "DoNothingModule.h"
#include "Strategist.h"

TEST(APMLimits, MultipleOrderSameUnit)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 500;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(115, 15))),
    };

    test.onFrameMine = []()
    {
        Strategist::setOpening({});

        auto getZealot = []()
        {
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Protoss_Zealot) return unit;
            }

            return (BWAPI::Unit)nullptr;
        };

        if (BWAPI::Broodwar->getFrameCount() == 450)
        {
            std::cout << "Zealot target: " << BWAPI::TilePosition(getZealot()->getOrderTargetPosition()) << std::endl;
        }

        if (BWAPI::Broodwar->getFrameCount() < 5 || BWAPI::Broodwar->getFrameCount() > 400) return;

        BWAPI::Unit zealot = getZealot();

        for (int i=0; i<200; i++)
        {
            zealot->move(BWAPI::Position(BWAPI::TilePosition(115, 20)));
        }

        zealot->move(BWAPI::Position(BWAPI::TilePosition(0 + BWAPI::Broodwar->getFrameCount() / 10, BWAPI::Broodwar->getFrameCount() % 10)));
    };

    test.run();
}
