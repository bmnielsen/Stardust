#include "BWTest.h"
#include "DoNothingModule.h"

#include "Map.h"
#include "Strategist.h"
#include "TestMainArmyAttackBasePlay.h"
#include "Plays/Macro/SaturateBases.h"

namespace
{
    class MacroOnlyStrategyEngine : public StrategyEngine
    {
        void initialize(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                              std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                              std::vector<std::pair<int, int>> &mineralReservations) override
        {
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);

            auto natural = Map::getMyNatural();
            if (natural && natural->ownedSince == -1 && BWAPI::Broodwar->getFrameCount() > 3000)
            {
                takeNaturalExpansion(plays, prioritizedProductionGoals);
            }
        }
    };
}

TEST(MaxSupply, StopsProduction)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 20000;

    // Kick-start our economy with lots of probes and some basic buildings
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(115, 8)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(109, 10)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(113, 10)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Cybernetics_Core, BWAPI::TilePosition(112, 8)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 6)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 7)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 8)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 9)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 10)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 11)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 12)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(123, 6)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(123, 7)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(123, 8)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(123, 9)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(123, 10)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(123, 11)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(123, 12)),
    };

    test.onStartMine = []()
    {
        Strategist::setStrategyEngine(std::make_unique<MacroOnlyStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<SaturateBases>());
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(
                Map::baseNear(BWAPI::Position(BWAPI::TilePosition(61, 63)))));
        Strategist::setOpening(openingPlays);
    };
    
    // TODO: Assert

    test.run();
}
