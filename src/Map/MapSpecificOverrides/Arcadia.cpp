#include "Arcadia.h"

#include "Map.h"
#include "Units.h"

void Arcadia::modifyBases(std::vector<Base *> &bases)
{
    // Top-right min-only

    // Hop out if we have the base
    for (auto &base : bases)
    {
        if (base->getTilePosition() == BWAPI::TilePosition(113, 40)) return;
    }

    const BWEM::Area *area = BWEM::Map::Instance().GetNearestArea(BWAPI::TilePosition(113, 40));

    std::vector<Resource> mineralPatches;

    auto addResource = [](BWAPI::TilePosition tile, std::vector<Resource> &resources)
    {
        auto resource = Units::resourceAt(tile);
        if (resource)
        {
            resources.emplace_back(resource);
        }
#if LOGGING_ENABLED
        else
        {
            Log::Get() << "ERROR: Cannot find our resource unit for resource @ " << tile;
        }
#endif
    };

    for (auto tile : {
            BWAPI::TilePosition(119, 36),
            BWAPI::TilePosition(120, 37),
            BWAPI::TilePosition(120, 39),
            BWAPI::TilePosition(121, 40),
            BWAPI::TilePosition(121, 42),
            BWAPI::TilePosition(120, 43),
    })
    {
        addResource(tile, mineralPatches);
    }

    bases.push_back(new Base(BWAPI::TilePosition(113, 40), area, mineralPatches, {}));
}

void Arcadia::modifyStartingLocation(std::unique_ptr<StartingLocation> &startingLocation)
{
    if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(8, 6))
    {
        startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(91, 187)));
    }
    else if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(116, 6))
    {
        startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(428, 173)));
    }
    else if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(8, 117))
    {
        startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(104, 317)));
    }
    else if (startingLocation->main->getTilePosition() == BWAPI::TilePosition(116, 117))
    {
        startingLocation->naturalChoke = Map::chokeNear(BWAPI::Position(BWAPI::WalkPosition(412, 324)));
    }
}

Base *Arcadia::naturalForWallPlacement(Base *main)
{
    if (main->getTilePosition() == BWAPI::TilePosition(8, 6))
    {
        return Map::baseNear(BWAPI::Position(BWAPI::TilePosition(10,37)));
    }
    if (main->getTilePosition() == BWAPI::TilePosition(116, 6))
    {
        return Map::baseNear(BWAPI::Position(BWAPI::TilePosition(113, 39)));
    }
    if (main->getTilePosition() == BWAPI::TilePosition(8, 117))
    {
        return Map::baseNear(BWAPI::Position(BWAPI::TilePosition(10,82)));
    }
    if (main->getTilePosition() == BWAPI::TilePosition(116, 117))
    {
        return Map::baseNear(BWAPI::Position(BWAPI::TilePosition(113,81)));
    }

    return nullptr;
}
