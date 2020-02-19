#include "BuildingPlacement.h"

#include <BWEB.h>

#include "Map.h"
#include "Builder.h"
#include "PathFinding.h"
#include "UnitUtil.h"

namespace BuildingPlacement
{
    namespace
    {
        std::vector<Neighbourhood> ALL_NEIGHBOURHOODS = {Neighbourhood::MainBase};

        std::map<Neighbourhood, BWAPI::Position> neighbourhoodOrigins;
        std::map<Neighbourhood, std::map<int, BuildLocationSet>> availableBuildLocations;

        BuildLocationSet _availableGeysers;

        // Checks if a building with the given type can be built at the given position.
        // If the building requires psi, this method will verify the position has power, unless skipPsiCheck is true.
        bool canBuildHere(
                BWAPI::UnitType type,
                BWAPI::TilePosition tile,
                bool skipPsiCheck = false,
                std::set<BWAPI::TilePosition> *reservedPositions = nullptr)
        {
            if (!skipPsiCheck && type.requiresPsi() && !BWAPI::Broodwar->hasPower(tile, type)) return false;
            if (type.isResourceDepot() && !BWAPI::Broodwar->canBuildHere(tile, type)) return false;

            for (auto x = tile.x; x < tile.x + type.tileWidth(); x++)
            {
                for (auto y = tile.y; y < tile.y + type.tileHeight(); y++)
                {
                    BWAPI::TilePosition currentTile(x, y);
                    if (!currentTile.isValid()) return false;
                    if (reservedPositions && reservedPositions->find(currentTile) != reservedPositions->end()) return false;
                    if (!BWAPI::Broodwar->isBuildable(currentTile)) return false;
                    if (BWEB::Map::isUsed(currentTile)) return false;
                    if (BWAPI::Broodwar->hasCreep(currentTile)) return false;
                }
            }

            return true;
        }

        bool canBuildHere(BWAPI::UnitType type, BWAPI::TilePosition tile, std::set<BWAPI::TilePosition> *reservedPositions)
        {
            return canBuildHere(type, tile, false, reservedPositions);
        }

        // At how many frames from now will the build position given by the tile and type be powered
        // If the position is already powered, returns 0
        // If the position will be powered by a pending pylon, returns the number of frames until the pylon is complete
        // Otherwise returns -1
        int poweredAfter(BWAPI::TilePosition tile, BWAPI::UnitType type, std::vector<Building *> &pendingPylons)
        {
            if (BWAPI::Broodwar->hasPower(tile, type)) return 0;

            int result = -1;
            for (auto &pendingPylon : pendingPylons)
            {
                if (UnitUtil::Powers(pendingPylon->tile, tile, type) &&
                    (result == -1 || pendingPylon->expectedFramesUntilCompletion() < result))
                {
                    result = pendingPylon->expectedFramesUntilCompletion();
                }
            }

            return result;
        }

        // Rebuilds the map of available build locations
        void updateAvailableBuildLocations()
        {
            std::map<Neighbourhood, std::map<int, BuildLocationSet>> result;

            // Examine our pending buildings to get:
            // - build locations that are about to be used
            // - incomplete pylons
            std::set<BWAPI::TilePosition> reservedTiles;
            std::vector<Building *> pendingPylons;
            for (auto building : Builder::allPendingBuildings())
            {
                if (!building->isConstructionStarted())
                {
                    reservedTiles.insert(building->tile);
                }

                if (building->type == BWAPI::UnitTypes::Protoss_Pylon)
                {
                    pendingPylons.push_back(building);
                }
            }

            // Scan blocks to:
            // - Collect the powered (or soon-to-be-powered) medium and large build locations we already have available
            // - Collect the next pylon to be built in each block
            for (auto &block : BWEB::Blocks::getBlocks())
            {
                // Consider medium building positions
                std::vector<std::tuple<BWAPI::TilePosition, int>> poweredMedium;
                std::vector<BWAPI::TilePosition> unpoweredMedium;
                for (auto tile : block.MediumTiles())
                {
                    if (canBuildHere(BWAPI::UnitTypes::Protoss_Forge, tile, true, &reservedTiles))
                    {
                        int framesToPower = poweredAfter(tile, BWAPI::UnitTypes::Protoss_Forge, pendingPylons);
                        if (framesToPower == -1)
                        {
                            unpoweredMedium.push_back(tile);
                        }
                        else
                        {
                            poweredMedium.emplace_back(tile, framesToPower);
                        }
                    }
                }

                // Consider large building positions
                std::vector<std::tuple<BWAPI::TilePosition, int>> poweredLarge;
                std::vector<BWAPI::TilePosition> unpoweredLarge;
                for (auto tile : block.LargeTiles())
                {
                    if (canBuildHere(BWAPI::UnitTypes::Protoss_Gateway, tile, true, &reservedTiles))
                    {
                        int framesToPower = poweredAfter(tile, BWAPI::UnitTypes::Protoss_Gateway, pendingPylons);
                        if (framesToPower == -1)
                        {
                            unpoweredLarge.push_back(tile);
                        }
                        else
                        {
                            poweredLarge.emplace_back(tile, framesToPower);
                        }
                    }
                }

                // Find the next pylon to build in this block
                // It is the available small tile location closest to the center of the block
                BWAPI::TilePosition blockCenter = block.Location() + BWAPI::TilePosition(block.width() / 2, block.height() / 2);
                int distBestToBlockCenter = INT_MAX;
                BWAPI::TilePosition pylonTile = BWAPI::TilePositions::Invalid;
                for (auto tile : block.SmallTiles())
                {
                    if (!canBuildHere(BWAPI::UnitTypes::Protoss_Pylon, tile, &reservedTiles)) continue;

                    int distToBlockCenter = tile.getApproxDistance(blockCenter);
                    if (distToBlockCenter < distBestToBlockCenter)
                        distBestToBlockCenter = distToBlockCenter, pylonTile = tile;
                }

                // If the block is full, continue now
                if (!pylonTile.isValid() && poweredMedium.empty() && poweredLarge.empty())
                {
                    continue;
                }

                // Add data from the block to appropriate neighbourhoods
                for (auto &neighbourhood : ALL_NEIGHBOURHOODS)
                {
                    // Make sure we don't die if we for some reason have an unconfigured neighbourhood
                    if (neighbourhoodOrigins.find(neighbourhood) == neighbourhoodOrigins.end()) continue;

                    // Get distance to the block
                    int dist = PathFinding::GetGroundDistance(
                            neighbourhoodOrigins[neighbourhood],
                            BWAPI::Position(blockCenter) + BWAPI::Position(16, 16),
                            BWAPI::Broodwar->self()->getRace().getWorker(),
                            PathFinding::PathFindingOptions::UseNearestBWEMArea);

                    // If the block is not connected or is too far away from the neighbourhood, skip it
                    if (dist == -1 || dist > 2000) continue;

                    if (pylonTile.isValid())
                    {
                        BuildLocation pylon(pylonTile, builderFrames(neighbourhood, pylonTile, BWAPI::UnitTypes::Protoss_Pylon), 0);
                        for (auto &tile : unpoweredMedium)
                        {
                            pylon.powersMedium.emplace(tile, builderFrames(neighbourhood, tile, BWAPI::UnitTypes::Protoss_Forge), 0);
                        }
                        for (auto &tile : unpoweredLarge)
                        {
                            pylon.powersLarge.emplace(tile, builderFrames(neighbourhood, tile, BWAPI::UnitTypes::Protoss_Gateway), 0);
                        }
                        result[neighbourhood][2].insert(pylon);
                    }

                    for (auto &tileAndPoweredAt : poweredMedium)
                    {
                        result[neighbourhood][3].emplace(
                                std::get<0>(tileAndPoweredAt),
                                builderFrames(neighbourhood, std::get<0>(tileAndPoweredAt), BWAPI::UnitTypes::Protoss_Forge),
                                std::get<1>(tileAndPoweredAt));
                    }

                    for (auto &tileAndPoweredAt : poweredLarge)
                    {
                        result[neighbourhood][4].emplace(
                                std::get<0>(tileAndPoweredAt),
                                builderFrames(neighbourhood, std::get<0>(tileAndPoweredAt), BWAPI::UnitTypes::Protoss_Gateway),
                                std::get<1>(tileAndPoweredAt));
                    }
                }
            }

            availableBuildLocations = result;
        }

        void updateAvailableGeysers()
        {
            _availableGeysers.clear();

            for (auto &base : Map::allBases())
            {
                if (base->owner != BWAPI::Broodwar->self()) continue;
                if (!base->resourceDepot || !base->resourceDepot->exists() || !base->resourceDepot->completed) continue;

                for (auto geyser : base->geysers())
                {
                    if (Builder::isPendingHere(geyser->getInitialTilePosition())) continue;

                    // TODO: Order in some logical way
                    _availableGeysers.emplace(geyser->getInitialTilePosition(), 0, 0);
                }
            }
        }

        void dumpHeatmap()
        {
#if CHERRYVIS_ENABLED
            // We dump a heatmap with the following values:
            // - No building: 0
            // - Large building: 2
            // - Medium building: 3
            // - Pylon: 4
            // - Defensive location: 5

            std::vector<long> blocks(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight(), 0);

            auto addLocation = [&blocks](BWAPI::TilePosition tile, int width, int height, int value)
            {
                for (int x = tile.x; x < tile.x + width; x++)
                {
                    for (int y = tile.y; y < tile.y + height; y++)
                    {
                        blocks[x + y * BWAPI::Broodwar->mapWidth()] = value;
                    }
                }
            };

            for (const auto &block : BWEB::Blocks::getBlocks())
            {
                for (auto large : block.LargeTiles())
                { addLocation(large, 4, 3, 2); }
                for (auto medium : block.MediumTiles())
                { addLocation(medium, 3, 2, 3); }
                for (auto small : block.SmallTiles())
                { addLocation(small, 2, 2, 4); }
            }

            for (const auto &station : BWEB::Stations::getStations())
            {
                for (auto small : station.DefenseLocations())
                { addLocation(small, 2, 2, 5); }
            }

            CherryVis::addHeatmap("Blocks", blocks, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
#endif
        }
    }

    void initialize()
    {
        neighbourhoodOrigins.clear();
        availableBuildLocations.clear();
        _availableGeysers.clear();

        BWEB::Map::onStart();

        // FIXME: Create wall
        // FIXME: Create proxy blocks
        // FIXME: Look into other BWEB changes that may affect us, like start blocks

        BWEB::Blocks::findBlocks();

        dumpHeatmap();

        neighbourhoodOrigins[Neighbourhood::MainBase] = Map::getMyMain()->mineralLineCenter;
    }

    void onUnitDestroy(BWAPI::Unit unit)
    {
        BWEB::Map::onUnitDestroy(unit);
    }

    void onUnitMorph(BWAPI::Unit unit)
    {
        BWEB::Map::onUnitMorph(unit);
    }

    void onUnitCreate(BWAPI::Unit unit)
    {
        BWEB::Map::onUnitDiscover(unit);
    }

    void onUnitDiscover(BWAPI::Unit unit)
    {
        BWEB::Map::onUnitDiscover(unit);
    }

    void update()
    {
        // TODO: Only update locations when it is needed
        updateAvailableBuildLocations();

        updateAvailableGeysers();
    }

    std::map<Neighbourhood, std::map<int, BuildLocationSet>> &getBuildLocations()
    {
        return availableBuildLocations;
    }

    BuildLocationSet &availableGeysers()
    {
        return _availableGeysers;
    }

    bool BuildLocationCmp::operator()(const BuildLocation &a, const BuildLocation &b) const
    {
        return a.framesUntilPowered < b.framesUntilPowered
               || (b.framesUntilPowered >= a.framesUntilPowered && a.builderFrames < b.builderFrames)
               || (b.framesUntilPowered >= a.framesUntilPowered && b.builderFrames >= a.builderFrames && a.tile < b.tile);
    }

    // Approximately how many frames it will take a builder to reach the given build position
    // TODO: Update location origins depending on whether there are workers at a base
    int builderFrames(Neighbourhood location, BWAPI::TilePosition tile, BWAPI::UnitType type)
    {
        if (neighbourhoodOrigins.find(location) == neighbourhoodOrigins.end()) return 0;

        return PathFinding::ExpectedTravelTime(
                neighbourhoodOrigins[location],
                BWAPI::Position(tile) + (BWAPI::Position(type.tileSize()) / 2),
                BWAPI::Broodwar->self()->getRace().getWorker(),
                PathFinding::PathFindingOptions::UseNearestBWEMArea);
    }
}
