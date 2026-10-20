#include "Colosseum.h"

#include "Map.h"
#include "Units.h"

#include <bwem.h>

void Colosseum::modifyBases(std::vector<Base *> &bases)
{
    /* BWEM misses the natural for the top-left base when run in openbw */

    /* Hop out if we have the base */
    for (auto &baseLoc : bases)
    {
        if (baseLoc->getTilePosition() == BWAPI::TilePosition(33, 17)) return;
    }

    const BWEM::Area *area = BWEM::Map::Instance().GetNearestArea(BWAPI::TilePosition(33, 17));

    std::vector<Resource> mineralPatches;
    std::vector<Resource> geysers;

    auto addResource = [](BWAPI::TilePosition tilePos, std::vector<Resource> &resources)
    {
        auto resource = UnitMgr::resourceAt(tilePos);
        if (resource)
        {
            resources.emplace_back(resource);
        }
#if LOGGING_ENABLED
        else
        {
            AppLog::Get() << "ERROR: Cannot find our resource unit for resource @ " << tilePos;
        }
#endif
    };

    for (auto tilePos : {
            BWAPI::TilePosition(40, 13),
            BWAPI::TilePosition(41, 14),
            BWAPI::TilePosition(41, 15),
            BWAPI::TilePosition(41, 17),
            BWAPI::TilePosition(40, 18),
            BWAPI::TilePosition(41, 19)
    })
    {
        addResource(tilePos, mineralPatches);
    }
    addResource(BWAPI::TilePosition(35, 12), geysers);

    bases.push_back(new Base(BWAPI::TilePosition(33, 17), area, mineralPatches, geysers));
}

void Colosseum::modifyStartingLocation(std::unique_ptr<StartingLocation> &startingLocation)
{
    if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(8, 12))
    {
        startingLocation->mainChoke = startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(91, 103)));
        startingLocation->natBase = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(33, 17)));
    }
    else if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(116, 12))
    {
        startingLocation->mainChoke = startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(421, 112)));
        startingLocation->natBase = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(107, 38)));
    }
    else if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(8, 113))
    {
        startingLocation->mainChoke = startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(100, 392)));
        startingLocation->natBase = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(13, 85)));
    }
    else if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(116, 114))
    {
        startingLocation->mainChoke = startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(387, 395)));
        startingLocation->natBase = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(89, 107)));
    }
}

std::optional<ForgeGatewayWall> Colosseum::getWall(BWAPI::TilePosition startTile)
{
    /* Colosseum has neutral creep that blocks where we would normally want to build our walls */
    /* This is not made available from BWAPI without vision on the tiles, so we are just hard-coding the walls */

    if (startTile == BWAPI::TilePosition(8, 12))
    {
        ForgeGatewayWall outcome;

        outcome.pylon = BWAPI::TilePosition(15, 23);
        outcome.forge = BWAPI::TilePosition(17, 26);
        outcome.gateway = BWAPI::TilePosition(17, 23);
        outcome.cannons.emplace_back(15, 25);
        outcome.cannons.emplace_back(15, 27);
        outcome.cannons.emplace_back(17, 21);
        outcome.cannons.emplace_back(15, 21);
        outcome.cannons.emplace_back(19, 21);
        outcome.cannons.emplace_back(17, 19);

        outcome.gapEnd1 = BWAPI::Position(BWAPI::TilePosition(20, 23)) + BWAPI::Position(16, 16);
        outcome.gapEnd2 = BWAPI::Position(BWAPI::TilePosition(22, 22)) + BWAPI::Position(16, 16);

        return outcome;
    }
    if (startTile == BWAPI::TilePosition(116, 12))
    {
        ForgeGatewayWall outcome;

        outcome.pylon = BWAPI::TilePosition(113, 27);
        outcome.forge = BWAPI::TilePosition(107, 28);
        outcome.gateway = BWAPI::TilePosition(107, 25);
        outcome.cannons.emplace_back(110, 28);
        outcome.cannons.emplace_back(111, 26);
        outcome.cannons.emplace_back(111, 24);
        outcome.cannons.emplace_back(109, 23);
        outcome.cannons.emplace_back(113, 25);
        outcome.cannons.emplace_back(113, 23);

        outcome.gapEnd1 = BWAPI::Position(BWAPI::TilePosition(107, 25)) + BWAPI::Position(16, 16);
        outcome.gapEnd2 = BWAPI::Position(BWAPI::WalkPosition(423, 98)) + BWAPI::Position(4, 4);

        return outcome;
    }
    if (startTile == BWAPI::TilePosition(8, 113))
    {
        ForgeGatewayWall outcome;

        outcome.pylon = BWAPI::TilePosition(16, 103);
        outcome.forge = BWAPI::TilePosition(20, 102);
        outcome.gateway = BWAPI::TilePosition(18, 99);
        outcome.cannons.emplace_back(18, 102);
        outcome.cannons.emplace_back(16, 101);
        outcome.cannons.emplace_back(18, 104);
        outcome.cannons.emplace_back(16, 99);
        outcome.cannons.emplace_back(14, 102);
        outcome.cannons.emplace_back(14, 100);

        outcome.gapEnd1 = BWAPI::Position(BWAPI::TilePosition(18, 99)) + BWAPI::Position(16, 16);
        outcome.gapEnd2 = BWAPI::Position(BWAPI::TilePosition(18, 97)) + BWAPI::Position(16, 16);

        return outcome;
    }
    if (startTile == BWAPI::TilePosition(116, 114))
    {
        ForgeGatewayWall outcome;

        outcome.pylon = BWAPI::TilePosition(106, 102);
        outcome.forge = BWAPI::TilePosition(101, 102);
        outcome.gateway = BWAPI::TilePosition(101, 99);
        outcome.cannons.emplace_back(104, 102);
        outcome.cannons.emplace_back(105, 100);
        outcome.cannons.emplace_back(107, 100);
        outcome.cannons.emplace_back(103, 104);
        outcome.cannons.emplace_back(105, 104);
        outcome.cannons.emplace_back(108, 102);

        outcome.gapEnd1 = BWAPI::Position(BWAPI::TilePosition(104, 99)) + BWAPI::Position(16, 16);
        outcome.gapEnd2 = BWAPI::Position(BWAPI::TilePosition(105, 97)) + BWAPI::Position(16, 16);

        return outcome;
    }

    return ForgeGatewayWall{};
}

Base *Colosseum::naturalForWallPlacement(Base *main)
{
    /* The main is the closest base to the wall */

    return main;
}
