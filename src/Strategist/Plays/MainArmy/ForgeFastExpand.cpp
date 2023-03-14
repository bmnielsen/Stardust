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
        auto addBuilding = [&](BWAPI::UnitType type, BWAPI::TilePosition tile)
        {
            if (Units::myBuildingAt(tile)) return;

            auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(tile),
                                                                  BuildingPlacement::builderFrames(Map::getMyMain()->getPosition(),
                                                                                                   tile,
                                                                                                   type),
                                                                  0,
                                                                  0);
            prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(
                    std::in_place_type<UnitProductionGoal>,
                    label,
                    type,
                    1,
                    1,
                    buildLocation);
        };

        auto &wall = BuildingPlacement::getForgeGatewayWall();
        addBuilding(BWAPI::UnitTypes::Protoss_Pylon, wall.pylon);
        addBuilding(BWAPI::UnitTypes::Protoss_Forge, wall.forge);
    }
}
