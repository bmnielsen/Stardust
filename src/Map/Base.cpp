#include "Base.h"

#include "Geo.h"
#include "PathFinding.h"
#include "Units.h"

namespace
{
    std::vector<BWAPI::Unit> getBlockingNeutrals(BWAPI::TilePosition tilePos)
    {
        std::vector<BWAPI::Unit> outcome;
        for (auto gameUnit : BWAPI::Broodwar->getStaticNeutralUnits())
        {
            if (gameUnit->getType().isMineralField())
            {
                if (Geom::Overlaps(tilePos - BWAPI::TilePosition(3, 3),
                                  10,
                                  9,
                                  gameUnit->getInitialTilePosition(),
                                  2,
                                  1))
                {
                    outcome.push_back(gameUnit);
                }
            }
            else
            {
                if (Geom::Overlaps(tilePos,
                                  4,
                                  3,
                                  gameUnit->getInitialTilePosition(),
                                  gameUnit->getType().tileWidth(),
                                  gameUnit->getType().tileHeight()))
                {
                    outcome.push_back(gameUnit);
                }
            }
        }

        return outcome;
    }

    void updateResourcesInSwitchPatchRange(std::vector<Resource> &mineralPatches)
    {
        for (auto &thisPatch : mineralPatches)
        {
            std::set<Resource> resourcesInSwitchPatchRange;

            auto tenDistancePositions = Geom::TenDistancePositionsAroundPatch(thisPatch->midpoint);

            for (auto &otherPatch : mineralPatches)
            {
                if (thisPatch == otherPatch) continue;

                bool inRange = false;
                for (auto &pt : tenDistancePositions)
                {
                    /* Game looks for a free patch within 8 tiles, ref: order_MoveToMinerals in bwgame.h */
                    if (Geom::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe, pt, BWAPI::UnitTypes::Resource_Mineral_Field, otherPatch->midpoint)
                        <= (32 * 8))
                    {
                        inRange = true;
                        break;
                    }
                }
                if (inRange) resourcesInSwitchPatchRange.insert(otherPatch);
            }

            thisPatch->resourcesInSwitchPatchRange = std::move(resourcesInSwitchPatchRange);
        }
    }
}

Base::Base(BWAPI::TilePosition tilePos, const BWEM::Base *bwemBase)
        : owner(nullptr)
        , resourceDepot(nullptr)
        , ownedSince(-1)
        , lastScouted(-1)
        , blockedByEnemy(false)
        , requiresMineralWalkFromEnemyStartLocations(false)
        , island(true) // set to false later where appropriate
        , workerDefenseRallyPatch(nullptr)
        , blockingNeutrals(getBlockingNeutrals(tilePos))
        , minerals(0)
        , gas(0)
        , tilePos(tilePos)
        , midpoint(BWAPI::Position(tilePos) + BWAPI::Position(64, 48))
        , bwemArea(bwemBase->GetArea())
{
    for (const auto &mineralPatch : bwemBase->Minerals())
    {
        auto resource = UnitMgr::resourceAt(mineralPatch->TopLeft());
        if (resource)
        {
            _mineralPatches.emplace_back(resource);
        }
#if LOGGING_ENABLED
        else
        {
            AppLog::Get() << "ERROR: Cannot find our resource unit for mineral field @ " << mineralPatch->TopLeft();
        }
#endif
    }

    for (const auto &geyser : bwemBase->Geysers())
    {
        auto resource = UnitMgr::resourceAt(geyser->TopLeft());
        if (resource)
        {
            _geysersOrRefineries.emplace_back(resource);
        }
#if LOGGING_ENABLED
        else
        {
            AppLog::Get() << "ERROR: Cannot find our resource unit for geyser @ " << geyser->TopLeft();
        }
#endif
    }

    /* Call update to set the minerals and gas counts */
    update();

    analyzeMineralLine();

    for (auto startLocationTile : BWAPI::Broodwar->getStartLocations())
    {
        if (Pathfinder::GetGroundDistance(BWAPI::Position(tilePos), BWAPI::Position(startLocationTile), BWAPI::UnitTypes::Protoss_Probe) != -1)
        {
            island = false;
            break;
        }
    }

    updateResourcesInSwitchPatchRange(_mineralPatches);
}

Base::Base(BWAPI::TilePosition tilePos, const BWEM::Area *bwemArea, std::vector<Resource> mineralPatches, std::vector<Resource> geysers)
        : owner(nullptr)
        , resourceDepot(nullptr)
        , ownedSince(-1)
        , lastScouted(-1)
        , blockedByEnemy(false)
        , requiresMineralWalkFromEnemyStartLocations(false)
        , island(true) // set to false later where appropriate
        , workerDefenseRallyPatch(nullptr)
        , blockingNeutrals(getBlockingNeutrals(tilePos))
        , minerals(0)
        , gas(0)
        , tilePos(tilePos)
        , midpoint(BWAPI::Position(tilePos) + BWAPI::Position(64, 48))
        , bwemArea(bwemArea)
        , _mineralPatches(std::move(mineralPatches))
        , _geysersOrRefineries(std::move(geysers))
{
    /* Call update to set the minerals and gas counts */
    update();

    analyzeMineralLine();

    for (auto startLocationTile : BWAPI::Broodwar->getStartLocations())
    {
        if (Pathfinder::GetGroundDistance(BWAPI::Position(tilePos), BWAPI::Position(startLocationTile), BWAPI::UnitTypes::Protoss_Probe) != -1)
        {
            island = false;
            break;
        }
    }

    updateResourcesInSwitchPatchRange(_mineralPatches);
}

bool Base::isStartingBase() const
{
    return std::find(
            BWAPI::Broodwar->getStartLocations().begin(),
            BWAPI::Broodwar->getStartLocations().end(),
            tilePos) != BWAPI::Broodwar->getStartLocations().end();
}

bool Base::isInMineralLine(BWAPI::TilePosition pt) const
{
    return mineralLineTiles.contains(pt);
}

bool Base::hasGeyserOrRefineryAt(BWAPI::TilePosition geyserTopLeft) const
{
    return std::any_of(_geysersOrRefineries.begin(), _geysersOrRefineries.end(), [&geyserTopLeft](const auto &geyserOrRefinery)
    {
        return geyserTopLeft == geyserOrRefinery->tilePos;
    });
}

bool Base::gasRequiresFourWorkers(BWAPI::TilePosition geyserTopLeft) const
{
    if (!hasGeyserOrRefineryAt(geyserTopLeft)) return false;

    return (geyserTopLeft.y - tilePos.y) > 5;
}

void Base::update()
{
    /* Count total minerals available and remove destroyed minerals */
    minerals = 0;
    for (auto iterRef = _mineralPatches.begin(); iterRef != _mineralPatches.end(); )
    {
        if ((*iterRef)->destroyed)
        {
            iterRef = _mineralPatches.erase(iterRef);
        }
        else
        {
            minerals += (*iterRef)->currentAmount;
            iterRef++;
        }
    }

    /* Count total gas available */
    gas = 0;
    for (const auto &geyserOrRefinery : _geysersOrRefineries)
    {
        gas += geyserOrRefinery->currentAmount;
    }
}

void Base::analyzeMineralLine()
{
    mineralLineTiles.clear();

    /* Compute the approximate center of the mineral line */
    if (mineralPatchCount() > 0)
    {
        size_t x = 0;
        size_t y = 0;
        for (const auto &mineral : _mineralPatches)
        {
            x += mineral->midpoint.x;
            y += mineral->midpoint.y;
        }

        mineralLineCenter = (getPosition() + BWAPI::Position((int)(x / mineralPatchCount()), (int)(y / mineralPatchCount())) * 2) / 3;
    }
    else
    {
        mineralLineCenter = getPosition();
    }

    /* Compute the tiles that are considered part of the mineral line */
    /* We do this by tracing lines from each mineral patch to the center of the resource depot and adding all surrounding tiles */
    {
        auto handleTile = [&](BWAPI::TilePosition input)
        {
            std::vector<BWAPI::TilePosition> tilesBetween;
            Geom::FindTilesBetween(input, BWAPI::TilePosition(getPosition()), tilesBetween);
            for (auto pt : tilesBetween)
            {
                if (Geom::EdgeToPointDistance(BWAPI::UnitTypes::Protoss_Nexus, getPosition(), BWAPI::Position(pt) + BWAPI::Position(16, 16))
                    < 1)
                    continue;

                for (int x = pt.x - 1; x <= pt.x + 1; x++)
                {
                    for (int y = pt.y - 1; y <= pt.y + 1; y++)
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
            handleTile(mineralPatch->tilePos);
            handleTile(mineralPatch->tilePos + BWAPI::TilePosition(1, 0));
        }
        for (const auto &geyser : _geysersOrRefineries)
        {
            handleTile(geyser->tilePos);
            handleTile(geyser->tilePos + BWAPI::TilePosition(1, 0));
            handleTile(geyser->tilePos + BWAPI::TilePosition(2, 0));
            handleTile(geyser->tilePos + BWAPI::TilePosition(3, 0));
            handleTile(geyser->tilePos + BWAPI::TilePosition(0, 1));
            handleTile(geyser->tilePos + BWAPI::TilePosition(1, 1));
            handleTile(geyser->tilePos + BWAPI::TilePosition(2, 1));
            handleTile(geyser->tilePos + BWAPI::TilePosition(3, 1));
        }
    }

    /* Compute the best mineral patch to use for rallying workers during worker defense */
    {
        /* Start with the closest patch to the mineral line center */
        int closestPatchDist = INT_MAX;
        for (const auto &mineralPatch : _mineralPatches)
        {
            int span = mineralPatch->getDistance(mineralLineCenter);
            if (span < closestPatchDist)
            {
                workerDefenseRallyPatch = mineralPatch;
                closestPatchDist = span;
            }
        }

        /* Now try to get a neighbouring mineral patch that is further away from the depot */
        if (workerDefenseRallyPatch)
        {
            int currentDist = workerDefenseRallyPatch->getDistance(BWAPI::UnitTypes::Protoss_Nexus, midpoint);
            for (const auto &mineralPatch : mineralPatches())
            {
                if (mineralPatch == workerDefenseRallyPatch) continue;
                if (mineralPatch->getDistance(workerDefenseRallyPatch) > 0) continue;

                int span = mineralPatch->getDistance(BWAPI::UnitTypes::Protoss_Nexus, midpoint);
                if (span > currentDist)
                {
                    workerDefenseRallyPatch = mineralPatch;
                    break;
                }
            }
        }
    }
}