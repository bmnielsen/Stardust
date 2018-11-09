#include "BuildingPlacer.h"

#include <BWEB.h>

#include "PathFinding.h"

namespace { auto & bwemMap = BWEM::Map::Instance(); }
namespace { auto & bwebMap = BWEB::Map::Instance(); }

namespace BuildingPlacer
{
    namespace
    {
        // Checks if a building with the given type can be build at the given position.
        // If the building requires psi, this method will verify the position has power, unless skipPsiCheck is true.
        bool canBuildHere(
            BWAPI::UnitType type,
            BWAPI::TilePosition tile,
            bool skipPsiCheck = false,
            std::set<BWAPI::TilePosition> * reservedPositions = nullptr)
        {
            if (!skipPsiCheck && type.requiresPsi() && !BWAPI::Broodwar->hasPower(tile, type)) return false;
            if (type.isResourceDepot() && !BWAPI::Broodwar->canBuildHere(tile, type)) return false;

            for (auto x = tile.x; x < tile.x + type.tileWidth(); x++)
                for (auto y = tile.y; y < tile.y + type.tileHeight(); y++)
                {
                    BWAPI::TilePosition tile(x, y);
                    if (!tile.isValid()) return false;
                    if (reservedPositions && reservedPositions->find(tile) != reservedPositions->end()) return false;
                    if (!BWAPI::Broodwar->isBuildable(tile)) return false;
                    if (bwebMap.usedGrid[x][y]) return false;
                    if (BWAPI::Broodwar->hasCreep(tile)) return false;
                }
        }
        bool canBuildHere(BWAPI::UnitType type, BWAPI::TilePosition tile, std::set<BWAPI::TilePosition> * reservedPositions)
        {
            return canBuildHere(type, tile, false, reservedPositions);
        }


        BWAPI::TilePosition getPylonLocation(
            BWAPI::UnitType type,
            BWAPI::TilePosition nearTile,
            std::set<BWAPI::TilePosition> * reservedPositions)
        {
            // FIXME: Migrate start block logic

            // Collect data about all of the blocks we have
            struct BlockData
            {
                int dist;
                int poweredMedium = 0;
                int poweredLarge = 0;
                BWAPI::TilePosition pylon = BWAPI::TilePositions::Invalid;
            };
            std::vector<BlockData> blocks;

            // Also keep track of how many powered building locations we currently have
            int poweredLarge = 0;
            int poweredMedium = 0;

            for (auto &block : bwebMap.Blocks())
            {
                BlockData blockData;

                // Count powered large building positions
                for (auto tile : block.LargeTiles())
                    if (canBuildHere(BWAPI::UnitTypes::Protoss_Gateway, tile, true))
                        if (BWAPI::Broodwar->hasPower(tile, BWAPI::UnitTypes::Protoss_Gateway))
                            poweredLarge++;
                        else
                            blockData.poweredLarge++;

                // Count powered medium building positions
                for (auto tile : block.MediumTiles())
                    if (canBuildHere(BWAPI::UnitTypes::Protoss_Forge, tile, true))
                        if (BWAPI::Broodwar->hasPower(tile, BWAPI::UnitTypes::Protoss_Forge))
                            poweredMedium++;
                        else
                            blockData.poweredMedium++;

                // Find the next pylon to build in this block
                // It is the available small tile location closest to the center of the block
                BWAPI::TilePosition blockCenter = block.Location() + BWAPI::TilePosition(block.width() / 2, block.height() / 2);
                int distBestToBlockCenter = INT_MAX;
                for (auto tile : block.SmallTiles())
                {
                    if (!canBuildHere(BWAPI::UnitTypes::Protoss_Pylon, tile, reservedPositions)) continue;

                    int distToBlockCenter = tile.getApproxDistance(blockCenter);
                    if (distToBlockCenter < distBestToBlockCenter)
                        distBestToBlockCenter = distToBlockCenter, blockData.pylon = tile;
                }

                // If all the pylons are already built, don't consider this block
                if (!blockData.pylon.isValid()) continue;

                // Now compute the distance
                blockData.dist = PathFinding::GetGroundDistance(
                    BWAPI::Position(nearTile) + BWAPI::Position(16, 16),
                    BWAPI::Position(blockData.pylon) + BWAPI::Position(32, 32),
                    BWAPI::UnitTypes::Protoss_Probe,
                    PathFinding::PathFindingOptions::UseNearestBWEMArea);

                // If this block isn't ground-connected to the desired position, don't consider it
                if (blockData.dist == -1) continue;

                // Add the block
                blocks.push_back(blockData);
            }

            // FIXME: When we have a new production system, implement this again
            // Check the production queue to find what type of locations we most need right now
            // Break when we reach the next pylon, we assume it will give power to later buildings
            int availableLarge = poweredLarge;
            int availableMedium = poweredMedium;
            std::vector<bool> priority; // true for large, false for medium

            /*
            const auto & queue = ProductionManager::Instance().getQueue();
            for (int i = queue.size() - 1; i >= 0; i--)
            {
                const auto & macroAct = queue[i].macroAct;

                // Only care about buildings
                if (!macroAct.isBuilding()) continue;

                // Break when we hit the next pylon
                if (macroAct.getUnitType() == BWAPI::UnitTypes::Protoss_Pylon && i < (queue.size() - 1))
                    break;

                // Don't count buildings like nexuses and assimilators
                if (!macroAct.getUnitType().requiresPsi()) continue;

                if (macroAct.getUnitType().tileWidth() == 4)
                {
                    if (availableLarge > 0)
                        availableLarge--;
                    else
                        priority.push_back(true);
                }
                else if (macroAct.getUnitType().tileWidth() == 3)
                {
                    if (availableMedium > 0)
                        availableMedium--;
                    else
                        priority.push_back(false);
                }
            }
            */

            // If we have no priority buildings in the queue, but have few available building locations, make them a priority
            // We don't want to queue a building and not have space for it
            if (priority.empty())
            {
                if (availableLarge == 0) priority.push_back(true);
                if (availableMedium == 0) priority.push_back(false);
                if (availableLarge == 1) priority.push_back(true);
            }

            // Score the blocks and pick the best one
            // The general idea is:
            // - Prefer a block in the same area and close to the desired position
            // - Give a bonus to blocks that provide powered locations we currently need

            double bestScore = DBL_MAX;
            BlockData * bestBlock = nullptr;
            for (auto & block : blocks)
            {
                // Base score is based on the distance
                double score = log(block.dist);

                // Penalize the block if it is in a different BWEM area from the desired position
                if (bwemMap.GetNearestArea(nearTile) != bwemMap.GetNearestArea(block.pylon)) score *= 2;

                // Give the score a bonus based on the locations it powers
                int poweredLocationBonus = 0;
                int blockAvailableLarge = block.poweredLarge;
                int blockAvailableMedium = block.poweredMedium;
                for (bool isLarge : priority)
                {
                    if (isLarge && blockAvailableLarge > 0)
                    {
                        poweredLocationBonus += 2;
                        blockAvailableLarge--;
                    }
                    else if (!isLarge && blockAvailableMedium > 0)
                    {
                        poweredLocationBonus += 2;
                        blockAvailableMedium--;
                    }
                    else
                        break;
                }

                // Reduce the score based on the location bonus
                score /= (double)(poweredLocationBonus + 1);

                if (score < bestScore)
                {
                    bestScore = score;
                    bestBlock = &block;
                }
            }

            if (bestBlock)
            {
                return bestBlock->pylon;
            }

            return BWAPI::TilePositions::Invalid;
        }
    }

    void initialize()
    {
        bwebMap.onStart();

        // FIXME: Create wall
        // FIXME: Create proxy blocks
        // FIXME: Look into other BWEB changes that may affect us, like start blocks

        bwebMap.findBlocks();
    }

    void onUnitDestroy(BWAPI::Unit unit)
    {
        bwebMap.onUnitDestroy(unit);
    }

    void onUnitMorph(BWAPI::Unit unit)
    {
        bwebMap.onUnitMorph(unit);
    }

    void onUnitCreate(BWAPI::Unit unit)
    {
        bwebMap.onUnitDiscover(unit);
    }

    void onUnitDiscover(BWAPI::Unit unit)
    {
        bwebMap.onUnitDiscover(unit);
    }

    BWAPI::TilePosition getBuildingLocation(
        BWAPI::UnitType type,
        BWAPI::TilePosition nearTile,
        std::set<BWAPI::TilePosition> * reservedPositions)
    {
        // Cannons are placed at station defense locations
        // Generally we will place them specifically without calling this method
        if (type == BWAPI::UnitTypes::Protoss_Photon_Cannon)
        {
            const BWEB::Station* station = bwebMap.getClosestStation(nearTile);
            for (auto tile : station->DefenseLocations())
                if (canBuildHere(type, tile, reservedPositions))
                    return tile;

            return BWAPI::TilePositions::Invalid;
        }

        // Pylons are special; we want to make sure we give powered build locations for buildings we need later
        if (type == BWAPI::UnitTypes::Protoss_Pylon)
            return getPylonLocation(type, nearTile, reservedPositions);

        // Everything else is placed at the nearest available location
        int bestDist = INT_MAX;
        BWAPI::TilePosition bestTile = BWAPI::TilePositions::Invalid;
        for (auto & block : bwebMap.Blocks())
            for (auto tile : type.tileWidth() == 4 ? block.LargeTiles() : block.MediumTiles())
            {
                int dist = tile.getApproxDistance(nearTile);
                if (dist < bestDist)
                {
                    bestDist = dist;
                    bestTile = tile;
                }
            }

        return bestTile;
    }
}
