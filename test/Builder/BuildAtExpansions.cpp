#include "BWTest.h"
#include "DoNothingModule.h"

#include "Map.h"
#include "Strategist.h"
#include "TestMainArmyAttackBasePlay.h"
#include "Plays/Macro/SaturateBases.h"
#include "Plays/Macro/TakeExpansion.h"

namespace
{
    class ZealotExpand : public StrategyEngine
    {
        void initialize(std::vector<std::shared_ptr<Play>> &plays, bool transitioningFromRandom) override
        {
        }

        void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override
        {
            auto natural = Map::getMyNatural();
            if (natural && natural->ownedSince == -1) return;

            for (auto &play : plays)
            {
                if (std::dynamic_pointer_cast<TakeExpansion>(play))
                {
                    return;
                }
            }

            for (const auto &expansion : Map::getUntakenExpansions())
            {
                auto play = std::make_shared<TakeExpansion>(expansion, 0);
                plays.emplace(plays.begin(), play);

                Log::Get() << "Queued expansion to " << play->depotPosition;
                CherryVis::log() << "Added TakeExpansion play for base @ " << BWAPI::WalkPosition(play->depotPosition);
                break;
            }
        }

        void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                              std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                              std::vector<std::pair<int, int>> &mineralReservations) override
        {
            auto natural = Map::getMyNatural();
            if (natural && natural->ownedSince == -1) takeNaturalExpansion(plays, prioritizedProductionGoals);

            if (BWAPI::Broodwar->getFrameCount() > 15000)
            {
                prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         "test",
                                                                         BWAPI::UnitTypes::Protoss_Scout,
                                                                         -1,
                                                                         -1);
            }
            else if (BWAPI::Broodwar->getFrameCount() > 10000)
            {
                prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         "test",
                                                                         BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                                         -1,
                                                                         -1);
            }
            else
            {
                prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         "test",
                                                                         BWAPI::UnitTypes::Protoss_Zealot,
                                                                         -1,
                                                                         -1);
            }
        }
    };

}

TEST(BuildAtExpansions, CanBuildAtExpansions)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 25000;
    test.expectWin = false;

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

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(120, 114)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(116, 114)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(118, 114), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(122, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(120, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(118, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(116, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(114, 110), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Spore_Colony, BWAPI::TilePosition(122, 108)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Spore_Colony, BWAPI::TilePosition(120, 108)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Spore_Colony, BWAPI::TilePosition(118, 108)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(116, 108)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(114, 108), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(122, 106)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Spore_Colony, BWAPI::TilePosition(120, 106)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Spore_Colony, BWAPI::TilePosition(118, 106)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Spore_Colony, BWAPI::TilePosition(116, 106)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(114, 106), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(122, 106)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(120, 106)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(118, 106)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(116, 106)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(114, 106)),
    };

    Base *baseToAttack = nullptr;

    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(118, 118)));

        Strategist::setStrategyEngine(std::make_unique<ZealotExpand>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<SaturateBases>());
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack, true));
        Strategist::setOpening(openingPlays);
    };

    // TODO: Assert

    test.run();
}
