#include "Outsider.h"

namespace
{
    auto islandAreaTiles = {
            BWAPI::TilePosition(9,8),
            BWAPI::TilePosition(29,6),
            BWAPI::TilePosition(23,119),
            BWAPI::TilePosition(120,70),
            BWAPI::TilePosition(120,51),
    };

    std::map<BWAPI::TilePosition, BWAPI::TilePosition> baseToNewLocation = {
            {BWAPI::TilePosition(7, 116), BWAPI::TilePosition(7, 105)},
            {BWAPI::TilePosition(35, 120), BWAPI::TilePosition(48, 120)},
            {BWAPI::TilePosition(119, 73), BWAPI::TilePosition(119, 84)},
            {BWAPI::TilePosition(119, 51), BWAPI::TilePosition(119, 40)},
            {BWAPI::TilePosition(35, 5), BWAPI::TilePosition(48, 5)},
            {BWAPI::TilePosition(7, 9), BWAPI::TilePosition(7, 20)}
    };
}

// On Outsider, BWEM doesn't handle the blocking mineral fields, so the areas behind them can appear accessible
// This hook manually marks all of the inaccessible areas as islands to work around this
void Outsider::addIslandAreas(std::set<const BWEM::Area *> &islandAreas)
{
    for (const auto &area : BWEM::Map::Instance().Areas())
    {
        auto tile = BWAPI::TilePosition(area.Top());
        for (auto islandAreaTile : islandAreaTiles)
        {
            if (islandAreaTile.getApproxDistance(tile) < 4)
            {
                islandAreas.insert(&area);
                break;
            }
        }
    }
}

void Outsider::modifyMainBaseBuildingPlacementAreas(std::set<const BWEM::Area *> &areas)
{
    // On Outsider we have additional areas behind our main base that are initially blocked by mineral fields
    // We don't want to use them for building placement though
    auto mainArea = BWEM::Map::Instance().GetArea(BWAPI::Broodwar->self()->getStartLocation());
    if (!mainArea)
    {
        Log::Get() << "ERROR: Start location doesn't have a BWEM area";
        return;
    }

    areas.clear();
    areas.insert(mainArea);
}

void Outsider::modifyBases(std::vector<Base *> &bases)
{
    // We currently don't have the functionality to take the bases behind the blocking mineral lines in the outside ring
    // So we convert them into mineral-only bases on the accessible side
    for (auto &base : bases)
    {
        auto it = baseToNewLocation.find(base->getTilePosition());
        if (it == baseToNewLocation.end()) continue;

        base->island = false;
        base->tile = it->second;
        base->bwemArea = BWEM::Map::Instance().GetNearestArea(it->second);
        base->_geysersOrRefineries.clear();
        base->update();
        base->analyzeMineralLine();
    }
}
