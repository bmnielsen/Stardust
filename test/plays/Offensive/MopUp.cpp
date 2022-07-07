#include "BWTest.h"
#include "DoNothingModule.h"

#include "StardustAIModule.h"
#include "Strategist.h"
#include "Builder.h"

namespace
{
    class HidePylonStrategyEngine : public StrategyEngine
    {
        void initialize(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                              std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                              std::vector<std::pair<int, int>> &mineralReservations) override
        {
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "test",
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       1);

            if (built) return;

            BWAPI::TilePosition pylonLocation(89, 116);
            if (Builder::isPendingHere(pylonLocation))
            {
                built = true;
                return;
            }

            auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(pylonLocation), 0, 0, 0);
            prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                     "test",
                                                                     BWAPI::UnitTypes::Protoss_Pylon,
                                                                       buildLocation);
        }

    private:
        bool built = false;
    };
}

TEST(MopUp, FindsHiddenPylon)
{
    BWTest test;
    test.map = Maps::GetOne("Andromeda");
    test.randomSeed = 42;
    test.opponentRace = BWAPI::Races::Protoss;
    test.expectWin = true;
    test.frameLimit = 20000;
    test.opponentModule = []()
    {
        return new StardustAIModule();
    };

    test.onStartOpponent = []()
    {
        Strategist::setStrategyEngine(std::make_unique<HidePylonStrategyEngine>());
    };

    test.run();
}

TEST(MopUp, AttacksFloatingBuilding)
{
    BWTest test;
    test.map = Maps::GetOne("Icarus");
    test.randomSeed = 42;
    test.opponentRace = BWAPI::Races::Terran;
    test.expectWin = true;
    test.frameLimit = 12000;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };

    test.myInitialUnits =
    {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(43,14))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(44,14))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(45,14))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(46,14))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(47,14))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(48,14))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(43,15))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(44,15))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(45,15))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(46,15))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43,16))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(44,16))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(45,16))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(46,16))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Observer, BWAPI::Position(BWAPI::TilePosition(41,96))),
    };

    test.opponentInitialUnits =
    {
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(41,95))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Barracks, BWAPI::Position(BWAPI::TilePosition(41,96))),
    };

    test.onFrameOpponent = []()
    {
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Terran_Barracks && !unit->isLifted())
            {
                unit->lift();
            }

            if (unit->getType().isWorker())
            {
                unit->move(BWAPI::Position(BWAPI::TilePosition(46,15)));
            }
        }
    };

    test.run();
}
