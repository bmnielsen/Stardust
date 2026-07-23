#include "BWTest.h"

#include "DoNothingModule.h"
#include "Map.h"
#include "Strategist.h"
#include "UAlbertaBotModule.h"
#include "Units.h"
#include "StrategyEngines/PvT.h"

TEST(EnemyProxyAtNatural, Bunkers)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Random;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "Tanks";
        return module;
    };
    test.map = Maps::GetOne("Circuit");
    test.randomSeed = 44046;
    test.frameLimit = 12000;
    test.expectWin = false;
    test.clearOpponentModuleAtFrame = 4000;

    test.onStartMine = []()
    {
        Strategist::setStrategyEngine(std::make_unique<PvT>(), "Normal");
    };

    test.onFrameMine = []()
    {
        if (BWAPI::Broodwar->getFrameCount() == 4000)
        {
            auto mainCenter = Map::getMyMain()->getPosition();
            for (auto &worker : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
            {
                if (worker->getDistance(mainCenter) > 1000)
                {
                    BWAPI::Broodwar->killUnit(worker->bwapiUnit);
                    return;
                }
            }
        }
    };

    auto proxyUnits = {
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(3561, 1020), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(3561, 1050), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(3561, 1080), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(3561, 1110), true),
    };
    auto proxyBuildings = {
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Bunker, BWAPI::Position(3632, 992), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Bunker, BWAPI::Position(3664, 1056), true)
    };

    test.onFrameOpponent = [&]()
    {
        if (BWAPI::Broodwar->getFrameCount() == 4501)
        {
            for (const auto &unitAndPosition : proxyUnits)
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), unitAndPosition.type, unitAndPosition.getCenterPosition());
            }
        }
        if (BWAPI::Broodwar->getFrameCount() == 4505)
        {
            for (const auto &unitAndPosition : proxyBuildings)
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), unitAndPosition.type, unitAndPosition.getCenterPosition());
            }
        }
        if (BWAPI::Broodwar->getFrameCount() == 4509)
        {
            std::vector<BWAPI::Unit> bunkers;
            std::vector<BWAPI::Unit> marines;
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Terran_Bunker) bunkers.emplace_back(unit);
                if (unit->getType() == BWAPI::UnitTypes::Terran_Marine) marines.emplace_back(unit);
            }
            if (bunkers.empty()) return;
            bool bunk = true;
            for (auto marine : marines)
            {
                marine->load(bunk ? *bunkers.begin() : *bunkers.rbegin());
                bunk = !bunk;
            }
        }
    };

    test.run();
}
