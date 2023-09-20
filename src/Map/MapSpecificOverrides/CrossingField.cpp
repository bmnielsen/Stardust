#include "CrossingField.h"

#include "Map.h"

void CrossingField::modifyStartingLocation(std::unique_ptr<StartingLocation> &startingLocation)
{
    if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(7, 27))
    {
        startingLocation->natural = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(5, 6)));
        startingLocation->mainChoke = startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(28, 195)));
    }
    else if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(117, 66))
    {
        startingLocation->natural = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(119, 87)));
        startingLocation->mainChoke = startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(484, 186)));
    }
}

std::optional<ForgeGatewayWall> CrossingField::getWall(BWAPI::TilePosition startTile)
{
    // Forward natural choke is too wide
    return ForgeGatewayWall{};
}
