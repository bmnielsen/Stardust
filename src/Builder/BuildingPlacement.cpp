#include "BuildingPlacement.h"

#include "Map.h"
#include "Builder.h"
#include "PathFinding.h"
#include "UnitUtil.h"
#include "Geo.h"
#include "Block.h"

#include "Blocks/StartNormalLeft.h"
#include "Blocks/StartNormalRight.h"
#include "Blocks/StartCompactLeft.h"
#include "Blocks/StartCompactRight.h"
#include "Blocks/StartCompactLeftHorizontal.h"
#include "Blocks/StartCompactRightVertical.h"
#include "Blocks/StartBottomLeftHorizontal.h"
#include "Blocks/StartTopLeftHorizontal.h"

#include "Blocks/18x6.h"
#include "Blocks/16x8.h"
#include "Blocks/17x6.h"
#include "Blocks/14x6.h"
#include "Blocks/12x8.h"
#include "Blocks/16x5.h"
#include "Blocks/18x3.h"
#include "Blocks/13x6.h"
#include "Blocks/10x6.h"
#include "Blocks/8x8.h"
#include "Blocks/14x3.h"
#include "Blocks/12x5.h"
#include "Blocks/10x3.h"
#include "Blocks/8x5.h"
#include "Blocks/4x8.h"
#include "Blocks/6x3.h"
#include "Blocks/4x5.h"
#include "Blocks/8x2.h"
#include "Blocks/5x4.h"
#include "Blocks/5x2.h"
#include "Blocks/4x4.h"
#include "Blocks/4x2.h"
#include "Blocks/2x4.h"
#include "Blocks/2x2.h"

namespace BuildingPlacement
{
    namespace
    {
        std::vector<Neighbourhood> ALL_NEIGHBOURHOODS = {Neighbourhood::MainBase};

        // Stores a bitmask for each tile
        // 1: not buildable
        // 2: adjacent to not buildable
        // 4: reserved for block
        // 8: adjacent to reserved for block
        std::vector<unsigned int> tileAvailability;

        std::shared_ptr<Block> startBlock;
        std::vector<std::shared_ptr<Block>> blocks;
        std::map<Base *, std::pair<BWAPI::TilePosition, std::vector<BWAPI::TilePosition>>> baseStaticDefenses;
        auto emptyBaseStaticDefenses = std::make_pair(BWAPI::TilePositions::Invalid, std::vector<BWAPI::TilePosition>{});

        bool updateRequired;
        std::map<Neighbourhood, std::set<const BWEM::Area *>> neighbourhoodAreas;
        std::map<Neighbourhood, BWAPI::Position> neighbourhoodOrigins;
        std::map<Neighbourhood, BWAPI::Position> neighbourhoodExits;
        std::map<Neighbourhood, std::map<int, BuildLocationSet>> availableBuildLocations;

        BuildLocationSet _availableGeysers;

        void initializeTileAvailability()
        {
            tileAvailability.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());

            auto markAdjacent = [](int tileX, int tileY)
            {
                if (tileX < 0 || tileY < 0 || tileX >= BWAPI::Broodwar->mapWidth() || tileY >= BWAPI::Broodwar->mapHeight()) return;

                tileAvailability[tileX + tileY * BWAPI::Broodwar->mapWidth()] |= 2U;
            };

            for (int tileX = 0; tileX < BWAPI::Broodwar->mapWidth(); tileX++)
            {
                for (int tileY = 0; tileY < BWAPI::Broodwar->mapHeight(); tileY++)
                {
                    if (!Map::isWalkable(tileX, tileY) || !BWAPI::Broodwar->isBuildable(tileX, tileY))
                    {
                        tileAvailability[tileX + tileY * BWAPI::Broodwar->mapWidth()] = 1;
                        markAdjacent(tileX - 1, tileY - 1);
                        markAdjacent(tileX + 0, tileY - 1);
                        markAdjacent(tileX + 1, tileY - 1);
                        markAdjacent(tileX + 1, tileY + 0);
                        markAdjacent(tileX + 1, tileY + 1);
                        markAdjacent(tileX + 0, tileY + 1);
                        markAdjacent(tileX - 1, tileY + 1);
                        markAdjacent(tileX - 1, tileY + 0);
                    }
                }
            }

            for (auto base : Map::allBases())
            {
                for (int tileX = base->getTilePosition().x - 1; tileX <= base->getTilePosition().x + 4; tileX++)
                {
                    for (int tileY = base->getTilePosition().y - 1; tileY <= base->getTilePosition().y + 3; tileY++)
                    {
                        if (tileX < 0 || tileY < 0 || tileX >= BWAPI::Broodwar->mapWidth() || tileY >= BWAPI::Broodwar->mapHeight()) continue;

                        if (tileX == base->getTilePosition().x - 1 || tileY == base->getTilePosition().y - 1 || tileX == base->getTilePosition().x + 4
                            || tileY == base->getTilePosition().y + 3)
                        {
                            tileAvailability[tileX + tileY * BWAPI::Broodwar->mapWidth()] = 2;
                        }
                        else
                        {
                            tileAvailability[tileX + tileY * BWAPI::Broodwar->mapWidth()] = 1;
                        }
                    }
                }
            }
        }

        void addBaseStaticDefense(Base *base, BWAPI::TilePosition pylon, std::vector<BWAPI::TilePosition> cannons)
        {
            auto markLocation = [](BWAPI::TilePosition tile)
            {
                for (int tileX = tile.x - 1; tileX <= tile.x + 2; tileX++)
                {
                    for (int tileY = tile.y - 1; tileY <= tile.y + 2; tileY++)
                    {
                        if (tileX < 0 || tileY < 0 || tileX >= BWAPI::Broodwar->mapWidth() || tileY >= BWAPI::Broodwar->mapHeight()) continue;

                        if (tileX == tile.x - 1 || tileY == tile.y - 1 || tileX == tile.x + 2 || tileY == tile.y + 2)
                        {
                            tileAvailability[tileX + tileY * BWAPI::Broodwar->mapWidth()] |= 8U;
                        }
                        else
                        {
                            tileAvailability[tileX + tileY * BWAPI::Broodwar->mapWidth()] |= 4U;
                        }
                    }
                }
            };

            markLocation(pylon);
            for (auto cannon : cannons)
            {
                markLocation(cannon);
            }

            baseStaticDefenses[base] = std::make_pair(pylon, std::move(cannons));
        }

        void findStartBlock()
        {
            std::vector<std::shared_ptr<Block>> startBlocks = {
                    std::make_shared<StartNormalLeft>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<StartNormalRight>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<StartCompactLeft>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<StartCompactRight>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<StartCompactLeftHorizontal>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<StartCompactRightVertical>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<StartBottomLeftHorizontal>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<StartTopLeftHorizontal>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
            };

            for (const auto &blockType : startBlocks)
            {
                auto block = blockType->tryCreate(BWAPI::Broodwar->self()->getStartLocation(), tileAvailability);
                if (block)
                {
                    startBlock = block;
                    blocks.push_back(block);
                    addBaseStaticDefense(Map::getMyMain(), block->powerPylon, block->cannons);
                    return;
                }
            }

            Log::Get() << "WARNING: No start block available";
        }

        void findBaseStaticDefenses()
        {
            for (auto &base : Map::allBases())
            {
                // Main is handled by start block, unless one couldn't be found
                if (base == Map::getMyMain() && baseStaticDefenses.find(base) != baseStaticDefenses.end()) continue;

                // Find the end mineral patches, which are the patches furthest away from each other
                BWAPI::Unit end1, end2;
                {
                    int maxDist = 0;
                    auto patches = base->mineralPatches();
                    for (auto first : patches)
                    {
                        for (auto second : patches)
                        {
                            int dist = first->getDistance(second);
                            if (dist > maxDist)
                            {
                                maxDist = dist;
                                end1 = first;
                                end2 = second;
                            }
                        }
                    }

                    // If for whatever reason this base has no mineral patches, continue
                    if (maxDist == 0) continue;
                }

                std::set<BWAPI::TilePosition> positions;
                auto addPositionIfValid = [&positions](BWAPI::TilePosition topLeft)
                {
                    for (int x = topLeft.x; x < topLeft.x + 2; x++)
                    {
                        for (int y = topLeft.y; y < topLeft.y + 2; y++)
                        {
                            if (!BWAPI::TilePosition(x, y).isValid()) return;
                            if ((tileAvailability[x + y * BWAPI::Broodwar->mapWidth()] & 1U) == 1) return;
                        }
                    }

                    positions.insert(topLeft);
                };
                for (int x = -2; x <= 4; x++)
                {
                    addPositionIfValid(base->getTilePosition() + BWAPI::TilePosition(x, -2));
                    addPositionIfValid(base->getTilePosition() + BWAPI::TilePosition(x, 3));
                }
                for (int y = -1; y <= 2; y++)
                {
                    addPositionIfValid(base->getTilePosition() + BWAPI::TilePosition(-2, y));
                    addPositionIfValid(base->getTilePosition() + BWAPI::TilePosition(4, y));
                }

                auto usePosition = [&positions](BWAPI::TilePosition tile)
                {
                    for (int x = tile.x - 1; x <= tile.x + 1; x++)
                    {
                        for (int y = tile.y - 1; y <= tile.y + 1; y++)
                        {
                            positions.erase(BWAPI::TilePosition(x, y));
                        }
                    }
                };

                std::vector<BWAPI::TilePosition> cannons;

                // Now place a cannon closest to each end
                auto placeEnd = [&](BWAPI::Unit end)
                {
                    int minDist = INT_MAX;
                    BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
                    for (auto tile : positions)
                    {
                        int dist = end->getPosition().getApproxDistance(BWAPI::Position(tile) + BWAPI::Position(16, 16));
                        if (dist < minDist)
                        {
                            minDist = dist;
                            best = tile;
                        }
                    }

                    if (best != BWAPI::TilePositions::Invalid)
                    {
                        usePosition(best);
                        cannons.push_back(best);
                    }

                    return best;
                };
                placeEnd(end1);
                placeEnd(end2);
                if (cannons.empty()) continue;

                // Now place the pylon so that it powers both and is as far as possible from minerals and geyser
                BWAPI::TilePosition pylon = BWAPI::TilePositions::Invalid;
                {
                    int geyserX = 0;
                    int geyserY = 0;
                    int geyserCount = 0;
                    for (auto geyser : base->geysers())
                    {
                        geyserX += geyser->getInitialPosition().x;
                        geyserY += geyser->getInitialPosition().y;
                        geyserCount++;
                    }
                    auto geyserPos = geyserCount == 0
                                     ? BWAPI::Positions::Invalid
                                     : BWAPI::Position(geyserX / geyserCount, geyserY / geyserCount);

                    int maxDist = 0;
                    for (auto tile : positions)
                    {
                        // First ensure the position powers the cannons
                        bool powersAll = true;
                        for (auto cannon : cannons)
                        {
                            if (!UnitUtil::Powers(tile, cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon))
                            {
                                powersAll = false;
                                break;
                            }
                        }
                        if (!powersAll) continue;

                        // Next check the distances
                        auto pos = BWAPI::Position(tile) + BWAPI::Position(16, 16);
                        int dist = pos.getApproxDistance(base->mineralLineCenter);
                        if (geyserCount > 0)
                        {
                            // Weight minerals twice as high as geyser
                            dist *= 2;
                            dist += pos.getApproxDistance(geyserPos);
                        }

                        if (dist > maxDist)
                        {
                            maxDist = dist;
                            pylon = tile;
                        }
                    }

                    if (!pylon.isValid()) continue;

                    usePosition(pylon);
                }

                // TODO: Place additional cannons

                addBaseStaticDefense(base, pylon, cannons);
            }
        }

        void findBlocks()
        {
            std::vector<std::shared_ptr<Block>> normalBlocks = {
                    std::make_shared<Block18x6>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block16x8>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block17x6>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block14x6>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block12x8>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block16x5>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block18x3>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block13x6>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block10x6>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block8x8>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block14x3>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block12x5>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block10x3>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block8x5>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block4x8>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block6x3>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block4x5>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block8x2>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block5x4>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block5x2>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block4x4>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block4x2>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block2x4>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<Block2x2>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
            };

            for (const auto &blockType : normalBlocks)
            {
                BWAPI::TilePosition center = BWAPI::TilePosition(
                        (BWAPI::Broodwar->mapWidth() - blockType->width()) / 2,
                        (BWAPI::Broodwar->mapHeight() - blockType->height()) / 2);

                for (int tileX = 0; tileX <= center.x; tileX++)
                {
                    for (int tileY = 0; tileY <= center.y; tileY++)
                    {
                        if (auto block = blockType->tryCreate(BWAPI::TilePosition(tileX, tileY), tileAvailability)) blocks.push_back(block);
                    }
                }

                for (int tileX = BWAPI::Broodwar->mapWidth() - blockType->width(); tileX > center.x; tileX--)
                {
                    for (int tileY = 0; tileY <= center.y; tileY++)
                    {
                        if (auto block = blockType->tryCreate(BWAPI::TilePosition(tileX, tileY), tileAvailability)) blocks.push_back(block);
                    }
                }

                for (int tileX = 0; tileX <= center.x; tileX++)
                {
                    for (int tileY = BWAPI::Broodwar->mapHeight() - blockType->height(); tileY > center.y; tileY--)
                    {
                        if (auto block = blockType->tryCreate(BWAPI::TilePosition(tileX, tileY), tileAvailability)) blocks.push_back(block);
                    }
                }

                for (int tileX = BWAPI::Broodwar->mapWidth() - blockType->width(); tileX > center.x; tileX--)
                {
                    for (int tileY = BWAPI::Broodwar->mapHeight() - blockType->height(); tileY > center.y; tileY--)
                    {
                        if (auto block = blockType->tryCreate(BWAPI::TilePosition(tileX, tileY), tileAvailability)) blocks.push_back(block);
                    }
                }
            }
        }

        int distanceToExit(Neighbourhood location, BWAPI::TilePosition tile, BWAPI::UnitType type)
        {
            if (neighbourhoodExits.find(location) == neighbourhoodExits.end()) return 0;

            auto dist = PathFinding::GetGroundDistance(
                    BWAPI::Position(tile) + (BWAPI::Position(type.tileSize()) / 2),
                    neighbourhoodExits[location],
                    BWAPI::UnitTypes::Protoss_Dragoon,
                    PathFinding::PathFindingOptions::UseNearestBWEMArea);

            // For the main base neighbourhood, prefer not to place buildings too close to the exit
            // Rationale: the main base exit is probably a choke and we don't want to have our buildings
            // get in the way of our choke defense
            if (location == Neighbourhood::MainBase && dist < 320)
            {
                dist = 320 + (320 - dist);
            }

            return dist;
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

            // Gather our pending pylons
            auto pendingPylons = Builder::pendingBuildingsOfType(BWAPI::UnitTypes::Protoss_Pylon);

            // Scan blocks to:
            // - Collect the powered (or soon-to-be-powered) medium and large build locations we have available
            // - Collect the next pylon to be built in each block
            for (auto &block : blocks)
            {
                // Consider medium building positions
                std::vector<std::tuple<Block::Location, int>> poweredMedium;
                std::vector<Block::Location> unpoweredMedium;
                for (auto placement : block->medium)
                {
                    int framesToPower = poweredAfter(placement.tile, BWAPI::UnitTypes::Protoss_Forge, pendingPylons);
                    if (framesToPower == -1)
                    {
                        unpoweredMedium.push_back(placement);
                    }
                    else
                    {
                        poweredMedium.emplace_back(placement, framesToPower);
                    }
                }

                // Consider large building positions
                std::vector<std::tuple<Block::Location, int>> poweredLarge;
                std::vector<Block::Location> unpoweredLarge;
                for (auto placement : block->large)
                {
                    int framesToPower = poweredAfter(placement.tile, BWAPI::UnitTypes::Protoss_Gateway, pendingPylons);
                    if (framesToPower == -1)
                    {
                        unpoweredLarge.push_back(placement);
                    }
                    else
                    {
                        poweredLarge.emplace_back(placement, framesToPower);
                    }
                }

                // If the block is full, continue now
                if (block->small.empty() && poweredMedium.empty() && poweredLarge.empty())
                {
                    continue;
                }

                // Add data from the block to appropriate neighbourhoods
                for (auto &neighbourhood : ALL_NEIGHBOURHOODS)
                {
                    // Make sure we don't die if we for some reason have an unconfigured neighbourhood
                    if (neighbourhoodAreas.find(neighbourhood) == neighbourhoodAreas.end()) continue;
                    if (neighbourhoodOrigins.find(neighbourhood) == neighbourhoodOrigins.end()) continue;
                    if (neighbourhoodExits.find(neighbourhood) == neighbourhoodExits.end()) continue;

                    auto it = neighbourhoodAreas[neighbourhood].find(
                            BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(block->center())));
                    if (it == neighbourhoodAreas[neighbourhood].end()) continue;

                    // Add pylons
                    for (auto pylonLocation : block->small)
                    {
                        BuildLocation pylon(pylonLocation,
                                            builderFrames(neighbourhood, block->small.begin()->tile, BWAPI::UnitTypes::Protoss_Pylon),
                                            0,
                                            distanceToExit(neighbourhood, block->small.begin()->tile, BWAPI::UnitTypes::Protoss_Pylon));

                        if (pylonLocation.tile == block->powerPylon)
                        {
                            for (auto &location : unpoweredMedium)
                            {
                                pylon.powersMedium.emplace(location,
                                                           builderFrames(neighbourhood, location.tile, BWAPI::UnitTypes::Protoss_Forge),
                                                           0,
                                                           distanceToExit(neighbourhood, location.tile, BWAPI::UnitTypes::Protoss_Forge));
                            }
                            for (auto &location : unpoweredLarge)
                            {
                                pylon.powersLarge.emplace(location,
                                                          builderFrames(neighbourhood, location.tile, BWAPI::UnitTypes::Protoss_Gateway),
                                                          0,
                                                          distanceToExit(neighbourhood, location.tile, BWAPI::UnitTypes::Protoss_Gateway));
                            }
                        }

                        result[neighbourhood][2].insert(pylon);
                    }

                    for (auto &tileAndPoweredAt : poweredMedium)
                    {
                        result[neighbourhood][3].emplace(
                                std::get<0>(tileAndPoweredAt),
                                builderFrames(neighbourhood, std::get<0>(tileAndPoweredAt).tile, BWAPI::UnitTypes::Protoss_Forge),
                                std::get<1>(tileAndPoweredAt),
                                distanceToExit(neighbourhood, std::get<0>(tileAndPoweredAt).tile, BWAPI::UnitTypes::Protoss_Forge));
                    }

                    for (auto &tileAndPoweredAt : poweredLarge)
                    {
                        result[neighbourhood][4].emplace(
                                std::get<0>(tileAndPoweredAt),
                                builderFrames(neighbourhood, std::get<0>(tileAndPoweredAt).tile, BWAPI::UnitTypes::Protoss_Gateway),
                                std::get<1>(tileAndPoweredAt),
                                distanceToExit(neighbourhood, std::get<0>(tileAndPoweredAt).tile, BWAPI::UnitTypes::Protoss_Gateway));
                    }
                }
            }

            availableBuildLocations = result;
        }

        void updateFramesUntilPowered()
        {
            // Gather our pending pylons
            auto pendingPylons = Builder::pendingBuildingsOfType(BWAPI::UnitTypes::Protoss_Pylon);

            // Loop and update every location with a current framesUntilPowered value
            for (auto &neighbourhoodAndLocations : availableBuildLocations)
            {
                for (auto &sizeAndLocations : neighbourhoodAndLocations.second)
                {
                    if (sizeAndLocations.first == 3 || sizeAndLocations.first == 4)
                    {
                        std::vector<BuildLocation> updatedLocations;
                        for (auto it = sizeAndLocations.second.begin(); it != sizeAndLocations.second.end();)
                        {
                            if (it->framesUntilPowered > 0)
                            {
                                updatedLocations.push_back(*it);
                                updatedLocations.rbegin()->framesUntilPowered = poweredAfter(
                                        it->location.tile,
                                        sizeAndLocations.first == 3 ? BWAPI::UnitTypes::Protoss_Forge : BWAPI::UnitTypes::Protoss_Gateway,
                                        pendingPylons);
                                it = sizeAndLocations.second.erase(it);
                            }
                            else
                            {
                                it++;
                            }
                        }

                        sizeAndLocations.second.insert(
                                std::make_move_iterator(updatedLocations.begin()),
                                std::make_move_iterator(updatedLocations.end()));
                    }
                }
            }
        }

        void updateAvailableGeysers()
        {
            _availableGeysers.clear();

            for (auto &base : Map::allBases())
            {
                if (base->owner != BWAPI::Broodwar->self()) continue;
                if (!base->resourceDepot || !base->resourceDepot->exists()) continue;
                if (!base->resourceDepot->completed &&
                    (base->resourceDepot->estimatedCompletionFrame - BWAPI::Broodwar->getFrameCount())
                    > UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Assimilator))
                {
                    continue;
                }

                for (auto geyser : base->geysers())
                {
                    if (Builder::isPendingHere(geyser->getInitialTilePosition())) continue;

                    // TODO: Order in some logical way
                    _availableGeysers.emplace(Block::Location(geyser->getInitialTilePosition()), 0, 0, 0);
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

            std::vector<long> blocksHeatmap(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight(), 0);

            auto addLocation = [&blocksHeatmap](BWAPI::TilePosition tile, int width, int height, int value)
            {
                for (int x = tile.x; x < tile.x + width; x++)
                {
                    if (x > BWAPI::Broodwar->mapWidth() - 1)
                    {
                        Log::Get() << "ERROR: BUILD LOCATION OUT OF BOUNDS @ " << tile;
                        continue;
                    }

                    for (int y = tile.y; y < tile.y + height; y++)
                    {
                        if (y > BWAPI::Broodwar->mapHeight() - 1)
                        {
                            Log::Get() << "ERROR: BUILD LOCATION OUT OF BOUNDS @ " << tile;
                            continue;
                        }

                        blocksHeatmap[x + y * BWAPI::Broodwar->mapWidth()] = value;
                    }
                }
            };

            for (const auto &block : blocks)
            {
                for (auto placement : block->large)
                {
                    if (!placement.converted) addLocation(placement.tile, 4, 3, 2);
                }
                for (auto placement : block->medium)
                {
                    if (!placement.converted) addLocation(placement.tile, 3, 2, 3);
                }
                for (auto placement : block->small)
                {
                    if (!placement.converted) addLocation(placement.tile, 2, 2, 4);
                }
            }

            for (const auto &baseAndStaticDefenses : baseStaticDefenses)
            {
                addLocation(baseAndStaticDefenses.second.first, 2, 2, 4);
                for (const auto &cannon : baseAndStaticDefenses.second.second)
                {
                    addLocation(cannon, 2, 2, 5);
                }
            }

            CherryVis::addHeatmap("Blocks", blocksHeatmap, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
#endif
        }
    }

    void initialize()
    {
        neighbourhoodAreas.clear();
        neighbourhoodOrigins.clear();
        neighbourhoodExits.clear();
        tileAvailability.clear();
        startBlock = nullptr;
        blocks.clear();
        baseStaticDefenses.clear();
        updateRequired = true;
        availableBuildLocations.clear();
        _availableGeysers.clear();

        neighbourhoodAreas[Neighbourhood::MainBase] = Map::getMyMainAreas();
        Map::mapSpecificOverride()->addMainBaseBuildingPlacementAreas(neighbourhoodAreas[Neighbourhood::MainBase]);
        neighbourhoodOrigins[Neighbourhood::MainBase] = Map::getMyMain()->mineralLineCenter;

        auto mainChoke = Map::getMyMainChoke();
        if (mainChoke)
        {
            neighbourhoodExits[Neighbourhood::MainBase] = mainChoke->center;
        }
        else
        {
            neighbourhoodExits[Neighbourhood::MainBase] = Map::getMyMain()->getPosition();
        }

        initializeTileAvailability();
        findStartBlock();
        findBaseStaticDefenses();
        findBlocks();

        dumpHeatmap();
    }

    void onBuildingQueued(const Building *building)
    {
        for (auto &block : blocks)
        {
            updateRequired = block->tilesReserved(building->tile, building->type.tileSize()) || updateRequired;
        }
    }

    void onBuildingCancelled(const Building *building)
    {
        for (auto &block : blocks)
        {
            updateRequired = block->tilesFreed(building->tile, building->type.tileSize()) || updateRequired;
        }
    }

    void onUnitCreate(const Unit &unit)
    {
        if (!unit->type.isBuilding()) return;

        for (auto &block : blocks)
        {
            updateRequired = block->tilesUsed(unit->getTilePosition(), unit->type.tileSize()) || updateRequired;
        }
    }

    void onUnitDestroy(const Unit &unit)
    {
        if (!unit->type.isBuilding()) return;

        for (auto &block : blocks)
        {
            updateRequired = block->tilesFreed(unit->getTilePosition(), unit->type.tileSize()) || updateRequired;
        }
    }

    void update()
    {
        if (updateRequired)
        {
            updateAvailableBuildLocations();
            updateRequired = false;
        }
        else
        {
            // We still need to update framesUntilPowered each frame
            updateFramesUntilPowered();
        }

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
        // Always sort the start block pylon before everything else
        if (startBlock && startBlock->powerPylon == a.location.tile) return true;
        if (startBlock && startBlock->powerPylon == b.location.tile) return false;

        // Prioritize locations that will be powered first
        if (a.framesUntilPowered < b.framesUntilPowered) return true;
        if (a.framesUntilPowered > b.framesUntilPowered) return false;

        // For medium locations, prioritize locations without an exit first - the producer will keep looking if it needs to place a building
        // with an exit
        if (!a.location.hasExit && b.location.hasExit) return true;
        if (a.location.hasExit && !b.location.hasExit) return false;

        // Then prioritize locations that have not been converted from a larger position
        if (!a.location.converted && b.location.converted) return true;
        if (a.location.converted && !b.location.converted) return false;

        // Finally prioritize by combined builder frames and distance to exit
        if ((a.builderFrames + a.distanceToExit) < (b.builderFrames + b.distanceToExit)) return true;
        if ((a.builderFrames + a.distanceToExit) > (b.builderFrames + b.distanceToExit)) return false;

        // Default case if everything else is equal
        return a.location.tile < b.location.tile;
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

    std::pair<BWAPI::TilePosition, std::vector<BWAPI::TilePosition>> &baseStaticDefenseLocations(Base *base)
    {
        auto it = baseStaticDefenses.find(base);
        return it == baseStaticDefenses.end() ? emptyBaseStaticDefenses : it->second;
    }
}
