#include "Base.h"

#include "Geo.h"
#include "PathFinding.h"
#include "Units.h"

namespace
{
    BWAPI::Unit getBlockingNeutral(BWAPI::TilePosition tile)
    {
        for (auto unit : BWAPI::Broodwar->getStaticNeutralUnits())
        {
            if (unit->getType().isMineralField())
            {
                if (Geo::Overlaps(tile - BWAPI::TilePosition(3, 3),
                                  10,
                                  9,
                                  unit->getInitialTilePosition(),
                                  2,
                                  1))
                {
                    return unit;
                }
            }
            else
            {
                if (Geo::Overlaps(tile,
                                  4,
                                  3,
                                  unit->getInitialTilePosition(),
                                  unit->getType().tileWidth(),
                                  unit->getType().tileHeight()))
                {
                    return unit;
                }
            }
        }

        return nullptr;
    }
}

Base::Base(BWAPI::TilePosition tile, const BWEM::Base *bwemBase)
        : owner(nullptr)
        , resourceDepot(nullptr)
        , ownedSince(-1)
        , lastScouted(-1)
        , blockedByEnemy(false)
        , requiresMineralWalkFromEnemyStartLocations(false)
        , island(true) // set to false later where appropriate
        , workerDefenseRallyPatch(nullptr)
        , blockingNeutral(getBlockingNeutral(tile))
        , minerals(0)
        , gas(0)
        , tile(tile)
        , center(BWAPI::Position(tile) + BWAPI::Position(64, 48))
        , bwemArea(bwemBase->GetArea())
{
    for (const auto &mineralPatch : bwemBase->Minerals())
    {
        auto resource = Units::resourceAt(mineralPatch->TopLeft());
        if (resource)
        {
            _mineralPatches.emplace_back(resource);
        }
#if LOGGING_ENABLED
        else
        {
            Log::Get() << "ERROR: Cannot find our resource unit for mineral field @ " << mineralPatch->TopLeft();
        }
#endif
    }

    for (const auto &geyser : bwemBase->Geysers())
    {
        auto resource = Units::resourceAt(geyser->TopLeft());
        if (resource)
        {
            _geysersOrRefineries.emplace_back(resource);
        }
#if LOGGING_ENABLED
        else
        {
            Log::Get() << "ERROR: Cannot find our resource unit for geyser @ " << geyser->TopLeft();
        }
#endif
    }

    // Call update to set the minerals and gas counts
    update();

    analyzeMineralLine();

    for (auto startLocationTile : BWAPI::Broodwar->getStartLocations())
    {
        if (PathFinding::GetGroundDistance(BWAPI::Position(tile), BWAPI::Position(startLocationTile), BWAPI::UnitTypes::Protoss_Probe) != -1)
        {
            island = false;
            break;
        }
    }
}

bool Base::isStartingBase() const
{
    return std::find(
            BWAPI::Broodwar->getStartLocations().begin(),
            BWAPI::Broodwar->getStartLocations().end(),
            tile) != BWAPI::Broodwar->getStartLocations().end();
}

bool Base::isInMineralLine(BWAPI::TilePosition pos) const
{
    return mineralLineTiles.find(pos) != mineralLineTiles.end();
}

bool Base::hasGeyserOrRefineryAt(BWAPI::TilePosition geyserTopLeft) const
{
    for (const auto &geyserOrRefinery : _geysersOrRefineries)
    {
        if (geyserTopLeft == geyserOrRefinery->tile) return true;
    }
    return false;
}

bool Base::gasRequiresFourWorkers(BWAPI::TilePosition geyserTopLeft) const
{
    if (!hasGeyserOrRefineryAt(geyserTopLeft)) return false;

    return (geyserTopLeft.y - tile.y) > 5;
}

void Base::update()
{
    // Count total minerals available and remove destroyed minerals
    minerals = 0;
    for (auto it = _mineralPatches.begin(); it != _mineralPatches.end(); )
    {
        if ((*it)->destroyed)
        {
            it = _mineralPatches.erase(it);
        }
        else
        {
            minerals += (*it)->currentAmount;
            it++;
        }
    }

    // Count total gas available
    gas = 0;
    for (const auto &geyserOrRefinery : _geysersOrRefineries)
    {
        gas += geyserOrRefinery->currentAmount;
    }
}

void Base::analyzeMineralLine()
{
    mineralLineTiles.clear();

    // Compute the approximate center of the mineral line
    if (mineralPatchCount() > 0)
    {
        int x = 0;
        int y = 0;
        for (const auto &mineral : _mineralPatches)
        {
            x += mineral->center.x;
            y += mineral->center.y;
        }

        mineralLineCenter = (getPosition() + BWAPI::Position(x / mineralPatchCount(), y / mineralPatchCount()) * 2) / 3;
    }
    else
    {
        mineralLineCenter = getPosition();
    }

    // Compute the tiles that are considered part of the mineral line
    // We do this by tracing lines from each mineral patch to the center of the resource depot and adding all surrounding tiles
    {
        auto handleTile = [&](BWAPI::TilePosition input)
        {
            std::vector<BWAPI::TilePosition> tilesBetween;
            Geo::FindTilesBetween(input, BWAPI::TilePosition(getPosition()), tilesBetween);
            for (auto pos : tilesBetween)
            {
                if (Geo::EdgeToPointDistance(BWAPI::UnitTypes::Protoss_Nexus, getPosition(), BWAPI::Position(pos) + BWAPI::Position(16, 16))
                    < 1)
                    continue;

                for (int x = pos.x - 1; x <= pos.x + 1; x++)
                {
                    for (int y = pos.y - 1; y <= pos.y + 1; y++)
                    {
                        BWAPI::TilePosition here(x, y);
                        if (here.isValid())
                        {
                            mineralLineTiles.insert(here);
                        }
                    }
                }
            }
        };
        for (const auto &mineralPatch : _mineralPatches)
        {
            handleTile(mineralPatch->tile);
            handleTile(mineralPatch->tile + BWAPI::TilePosition(1, 0));
        }
        for (const auto &geyser : _geysersOrRefineries)
        {
            handleTile(geyser->tile);
            handleTile(geyser->tile + BWAPI::TilePosition(1, 0));
            handleTile(geyser->tile + BWAPI::TilePosition(2, 0));
            handleTile(geyser->tile + BWAPI::TilePosition(3, 0));
            handleTile(geyser->tile + BWAPI::TilePosition(0, 1));
            handleTile(geyser->tile + BWAPI::TilePosition(1, 1));
            handleTile(geyser->tile + BWAPI::TilePosition(2, 1));
            handleTile(geyser->tile + BWAPI::TilePosition(3, 1));
        }
    }

    // Compute the best mineral patch to use for rallying workers during worker defense
    {
        // Start with the closest patch to the mineral line center
        int closestPatchDist = INT_MAX;
        for (const auto &mineralPatch : _mineralPatches)
        {
            int dist = mineralPatch->getDistance(mineralLineCenter);
            if (dist < closestPatchDist)
            {
                workerDefenseRallyPatch = mineralPatch;
                closestPatchDist = dist;
            }
        }

        // Now try to get a neighbouring mineral patch that is further away from the depot
        if (workerDefenseRallyPatch)
        {
            int currentDist = workerDefenseRallyPatch->getDistance(BWAPI::UnitTypes::Protoss_Nexus, center);
            for (const auto &mineralPatch : mineralPatches())
            {
                if (mineralPatch == workerDefenseRallyPatch) continue;
                if (mineralPatch->getDistance(workerDefenseRallyPatch) > 0) continue;

                int dist = mineralPatch->getDistance(BWAPI::UnitTypes::Protoss_Nexus, center);
                if (dist > currentDist)
                {
                    workerDefenseRallyPatch = mineralPatch;
                    break;
                }
            }
        }
    }
}