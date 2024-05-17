#include "BWTest.h"

#include "DoNothingModule.h"

namespace
{
    // Module that hallucinates one of our goons
    class HallucinateModule : public DoNothingModule
    {
    private:
        void onFrame() override
        {
            BWAPI::Unit ht = nullptr;
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Protoss_High_Templar)
                {
                    ht = unit;
                }
            }

            if (!ht) return;
            if (ht->getEnergy() < 100) return;

            for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon)
                {
                    ht->useTech(BWAPI::TechTypes::Hallucination, unit);
                    return;
                }
            }
        }
    };
}

TEST(Hallucination, EnemyHallucinatesUs)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new HallucinateModule();
    };
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Tau");
    test.randomSeed = 65145;
    test.frameLimit = 5000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(41, 31))),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_High_Templar, BWAPI::Position(BWAPI::TilePosition(41, 36))),
    };

    test.onStartOpponent = []()
    {
        BWAPI::Broodwar->self()->setResearched(BWAPI::TechTypes::Hallucination, true);
    };

    test.onFrameMine = []()
    {
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            unit->stop();
        }
    };

    test.run();
}
