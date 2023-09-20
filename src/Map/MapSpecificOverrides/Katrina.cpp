#include "Katrina.h"

#include "Map.h"

void Katrina::modifyStartingLocation(std::unique_ptr<StartingLocation> &startingLocation)
{
    if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(56, 5))
    {
        startingLocation->natural = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(8, 7)));
        startingLocation->mainChoke = startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(255, 39)));
    }
    else if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(118, 49))
    {
        startingLocation->natural = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(116, 7)));
        startingLocation->mainChoke = startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(473, 238)));
    }
    else if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(6, 72))
    {
        startingLocation->natural = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(8, 117)));
        startingLocation->mainChoke = startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(37, 261)));
    }
    else if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(72, 119))
    {
        startingLocation->natural = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(114, 116)));
        startingLocation->mainChoke = startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(254, 465)));
    }
}

std::optional<ForgeGatewayWall> Katrina::getWall(BWAPI::TilePosition startTile)
{
    // We could wall next to the main, but it currently overlaps the main start block.
    // It is not considered worth it to pursue right now.
    return ForgeGatewayWall{};
}
