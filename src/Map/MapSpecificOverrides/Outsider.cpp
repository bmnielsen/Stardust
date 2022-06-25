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
