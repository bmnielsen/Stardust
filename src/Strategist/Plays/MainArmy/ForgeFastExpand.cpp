#include "ForgeFastExpand.h"

#include "Geo.h"
#include "General.h"
#include "Units.h"
#include "Map.h"

ForgeFastExpand::ForgeFastExpand()
        : MainArmyPlay("ForgeFastExpand")
        , squad(std::make_shared<DefendWallSquad>())
{
    General::addSquad(squad);
}

void ForgeFastExpand::update()
{

}

void ForgeFastExpand::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Handle early build order
    if (currentFrame < 2500)
    {
        // Pylon and forge are inserted into the early production
        auto &wall = BuildingPlacement::getForgeGatewayWall();
        auto pylon = Units::myBuildingAt(wall.pylon);
        auto forge = Units::myBuildingAt(wall.forge);
        if (!pylon || !forge)
        {
            // First get the production goal producing the probes
            if (prioritizedProductionGoals.empty())
            {
                Log::Get() << "ERROR: No production goals for initial FFE build order, expecting probe production";
                return;
            }
            auto &priorityAndGoals = *prioritizedProductionGoals.begin();
            auto &goals = priorityAndGoals.second;

            auto insertAtSupply = [&](BWAPI::UnitType type, BWAPI::TilePosition tile, int supply)
            {
                auto it = goals.begin();

                auto insertBuilding = [&]()
                {
                    auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(tile),
                                                                          BuildingPlacement::builderFrames(Map::getMyMain()->getPosition(),
                                                                                                           tile,
                                                                                                           type),
                                                                          0,
                                                                          0);
                    it = goals.emplace(it, std::in_place_type<UnitProductionGoal>,
                                       label,
                                       type,
                                       1,
                                       1,
                                       buildLocation);
                };

                // Just build the building if we already have enough probes
                auto probesUntilBuilding = (supply * 2 - BWAPI::Broodwar->self()->supplyUsed()) / 2;
                if (probesUntilBuilding <= 0)
                {
                    insertBuilding();
                    return;
                }

                // Now ensure there is a probe goal first, otherwise just insert the building and bail out
                if (it == goals.end())
                {
                    insertBuilding();
                    return;
                }
                auto probeGoal = std::get_if<UnitProductionGoal>(&*it);
                if (!probeGoal || !probeGoal->unitType().isWorker())
                {
                    insertBuilding();
                    return;
                }

                // Now handle the case where we need to build all of the probes
                if (probeGoal->countToProduce() <= probesUntilBuilding)
                {
                    std::advance(it, 1);
                    insertBuilding();
                    return;
                }

                // Finally handle the case where we split the probe goals into two with the building in the middle
                auto desiredProbes = probeGoal->countToProduce();
                auto probeProducerLimit = probeGoal->getProducerLimit();
                auto probeLocation = probeGoal->getLocation();
                auto probeRequester = probeGoal->requester;

                it = goals.erase(it);

                it = goals.emplace(it, std::in_place_type<UnitProductionGoal>,
                                   probeRequester,
                                   BWAPI::UnitTypes::Protoss_Probe,
                                   desiredProbes - probesUntilBuilding,
                                   probeProducerLimit,
                                   probeLocation);

                insertBuilding();

                it = goals.emplace(it, std::in_place_type<UnitProductionGoal>,
                                   probeRequester,
                                   BWAPI::UnitTypes::Protoss_Probe,
                                   probesUntilBuilding,
                                   probeProducerLimit,
                                   probeLocation);
            };

            if (!forge) insertAtSupply(BWAPI::UnitTypes::Protoss_Forge, wall.forge, 10);
            if (!pylon) insertAtSupply(BWAPI::UnitTypes::Protoss_Pylon, wall.pylon, 8);
            return;
        }

    }

}
