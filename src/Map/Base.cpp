#include "Base.h"

#include "Geo.h"

Base::Base(BWAPI::TilePosition _tile, const BWEM::Base *_bwemBase)
        : owner(nullptr)
        , resourceDepot(nullptr)
        , ownedSince(-1)
        , lastScouted(-1)
        , spiderMined(false)
        , requiresMineralWalkFromEnemyStartLocations(false)
        , workerDefenseRallyPatch(nullptr)
        , tile(_tile)
        , bwemBase(_bwemBase)
{
    analyzeMineralLine();
}

std::vector<BWAPI::Unit> Base::mineralPatches() const
{
    std::vector<BWAPI::Unit> result;
    for (auto mineral : bwemBase->Minerals())
    {
        result.push_back(mineral->Unit());
    }

    return result;
}

std::vector<BWAPI::Unit> Base::geysers() const
{
    std::vector<BWAPI::Unit> result;
    for (auto geyser : bwemBase->Geysers())
    {
        if (geyser->Unit()->getType() != BWAPI::UnitTypes::Resource_Vespene_Geyser) continue;
        result.push_back(geyser->Unit());
    }

    return result;
}

std::vector<BWAPI::Unit> Base::refineries() const
{
    std::vector<BWAPI::Unit> result;
    for (auto geyser : bwemBase->Geysers())
    {
        if (geyser->Unit()->getPlayer() != BWAPI::Broodwar->self()) continue;
        result.push_back(geyser->Unit());
    }

    return result;
}

int Base::minerals() const
{
    int sum = 0;
    for (auto mineral : bwemBase->Minerals())
    {
        sum += mineral->Amount();
    }

    return sum;
}

int Base::gas() const
{
    int sum = 0;
    for (auto geyser : bwemBase->Geysers())
    {
        sum += geyser->Amount();
    }

    return sum;
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

void Base::analyzeMineralLine()
{
    // Compute the approximate center of the mineral line
    {
        int x = 0;
        int y = 0;
        for (auto mineral : bwemBase->Minerals())
        {
            x += mineral->Unit()->getPosition().x;
            y += mineral->Unit()->getPosition().y;
        }

        mineralLineCenter = (getPosition() + BWAPI::Position(x / mineralPatchCount(), y / mineralPatchCount()) * 2) / 3;
    }

    // Compute the tiles that are considered part of the mineral line
    // We do this by tracing lines from each mineral patch to the center of the resource depot and adding all surrounding tiles
    {
        for (auto mineralPatch : mineralPatches())
        {
            std::vector<BWAPI::TilePosition> tilesBetween;
            Geo::FindTilesBetween(BWAPI::TilePosition(mineralPatch->getPosition()), BWAPI::TilePosition(getPosition()), tilesBetween);
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
        }

        // Assumption is we shouldn't add the tiles between the refinery and depot, but can be added here if necessary
    }

    // Compute the best mineral patch to use for rallying workers during worker defense
    {
        // Start with the closest patch to the mineral line center
        int closestPatchDist = INT_MAX;
        for (auto mineralPatch : mineralPatches())
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
            int currentDist = Geo::EdgeToEdgeDistance(
                    BWAPI::UnitTypes::Protoss_Nexus,
                    getPosition(),
                    BWAPI::UnitTypes::Resource_Mineral_Field,
                    workerDefenseRallyPatch->getPosition());
            for (auto mineralPatch : mineralPatches())
            {
                if (mineralPatch == workerDefenseRallyPatch) continue;
                if (mineralPatch->getDistance(workerDefenseRallyPatch) > 0) continue;

                int dist = Geo::EdgeToEdgeDistance(
                        BWAPI::UnitTypes::Protoss_Nexus,
                        getPosition(),
                        BWAPI::UnitTypes::Resource_Mineral_Field,
                        mineralPatch->getPosition());
                if (dist > currentDist)
                {
                    workerDefenseRallyPatch = mineralPatch;
                    break;
                }
            }
        }
    }

#if CHERRYVIS_ENABLED
    // Dump to CherryVis
    std::vector<long> mineralLineCvis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
    for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
    {
        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
        {
            BWAPI::TilePosition here(x, y);

            if (workerDefenseRallyPatch && workerDefenseRallyPatch->getTilePosition() == here)
            {
                mineralLineCvis[x + y * BWAPI::Broodwar->mapWidth()] = 10;
            }
            else if (BWAPI::TilePosition(mineralLineCenter) == here)
            {
                mineralLineCvis[x + y * BWAPI::Broodwar->mapWidth()] = 10;
            }
            else if (isInMineralLine(here))
            {
                mineralLineCvis[x + y * BWAPI::Broodwar->mapWidth()] = -10;
            }
            else
            {
                mineralLineCvis[x + y * BWAPI::Broodwar->mapWidth()] = 0;
            }
        }
    }

    CherryVis::addHeatmap((std::ostringstream() << "MineralLine@" << tile).str(),
                          mineralLineCvis,
                          BWAPI::Broodwar->mapWidth(),
                          BWAPI::Broodwar->mapHeight());
#endif
}