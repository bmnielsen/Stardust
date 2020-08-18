#include "BWTest.h"
#include "DoNothingModule.h"

#include "Strategist.h"

// Doesn't work, can't reproduce the exact circumstances that cause the unit to stick

namespace
{
    class DoNothingStrategyEngine : public StrategyEngine
    {
        void initialize(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                              std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                              std::vector<std::pair<int, int>> &mineralReservations) override
        {}
    };
}

TEST(StuckDarkTemplar, AttackAndMove)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Tau Cross");
    test.randomSeed = 42;
    test.frameLimit = 500;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dark_Templar, BWAPI::Position(BWAPI::TilePosition(42, 30)) + BWAPI::Position(3,18))
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 32)))
    };

    // Order the dragoon to attack the bottom base
    test.onStartMine = []()
    {
        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());
        Strategist::setOpening({});

        std::cout << "Dark templar range: " << BWAPI::UnitTypes::Protoss_Dark_Templar.groundWeapon().maxRange() << std::endl;
    };

    test.onFrameOpponent = []()
    {
        // Move dragoon on frame 5
        if (BWAPI::Broodwar->getFrameCount() == 5)
        {
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon)
                {
                    unit->move(BWAPI::Position(BWAPI::TilePosition(46, 39)));
                }
            }
        }
    };

    test.onFrameMine = []()
    {
        // Send attack command on frame 5
        if (BWAPI::Broodwar->getFrameCount() == 5)
        {
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar)
                {
                    for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
                    {
                        if (enemy->getType() == BWAPI::UnitTypes::Protoss_Dragoon)
                        {
                            unit->attack(enemy);
                        }
                    }
                }
            }
        }

        // Send move command on frame 9
        if (BWAPI::Broodwar->getFrameCount() == 9)
        {
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar)
                {
                    unit->move(BWAPI::Position(BWAPI::TilePosition(43, 34)));
                }
            }
        }
    };

    test.run();
}
