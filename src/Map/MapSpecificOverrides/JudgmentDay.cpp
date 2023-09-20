#include "JudgmentDay.h"

#include "Map.h"

void JudgmentDay::modifyStartingLocation(std::unique_ptr<StartingLocation> &startingLocation)
{
    if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(7, 6))
    {
        startingLocation->natural = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(9, 34)));
        startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(151, 112)));
    }
    else if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(116, 7))
    {
        startingLocation->natural = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(115, 34)));
        startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(353, 111)));
    }
    else if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(8, 118))
    {
        startingLocation->natural = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(9, 92)));
        startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(157, 403)));
    }
    else if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(116, 118))
    {
        startingLocation->natural = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(115, 92)));
        startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(355, 405)));
    }
}

Base *JudgmentDay::naturalForWallPlacement(Base *main)
{
    if (main->getTilePosition() == BWAPI::TilePosition(7, 6))
    {
        return Map::baseNear(BWAPI::Position(BWAPI::TilePosition(30,24)));
    }
    if (main->getTilePosition() == BWAPI::TilePosition(116, 7))
    {
        return Map::baseNear(BWAPI::Position(BWAPI::TilePosition(94,24)));
    }
    if (main->getTilePosition() == BWAPI::TilePosition(8, 118))
    {
        return Map::baseNear(BWAPI::Position(BWAPI::TilePosition(30,102)));
    }
    if (main->getTilePosition() == BWAPI::TilePosition(116, 118))
    {
        return Map::baseNear(BWAPI::Position(BWAPI::TilePosition(95,102)));
    }

    return nullptr;
}
