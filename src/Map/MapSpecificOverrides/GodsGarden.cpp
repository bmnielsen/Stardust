#include "GodsGarden.h"

#include "Map.h"

void GodsGarden::modifyStartingLocation(std::unique_ptr<StartingLocation> &startingLocation)
{
    if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(7, 6))
    {
        startingLocation->natural = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(43, 8)));
        startingLocation->mainChoke = startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(50, 99)));
    }
    else if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(115, 24))
    {
        startingLocation->natural = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(115, 50)));
        startingLocation->mainChoke = startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(402, 40)));
    }
    else if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(9, 108))
    {
        startingLocation->natural = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(8, 77)));
        startingLocation->mainChoke = startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(108, 467)));
    }
    else if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(116, 117))
    {
        startingLocation->natural = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(85, 117)));
        startingLocation->mainChoke = startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(459, 386)));
    }
}

std::optional<ForgeGatewayWall> GodsGarden::getWall(BWAPI::TilePosition startTile)
{
    // Forward natural choke is too wide
    return ForgeGatewayWall{};
}
