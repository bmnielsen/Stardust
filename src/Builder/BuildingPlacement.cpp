#include "BuildingPlacement.h"

#include "Map.h"
#include "Builder.h"
#include "PathFinding.h"
#include "UnitUtil.h"
#include "Geo.h"
#include "Block.h"
#include "Strategist.h"
#include "Units.h"

#include "Blocks/StartNormalLeft.h"
#include "Blocks/StartNormalRight.h"
#include "Blocks/StartCompactLeft.h"
#include "Blocks/StartCompactRight.h"
#include "Blocks/StartCompactLeftHorizontal.h"
#include "Blocks/StartCompactRightVertical.h"
#include "Blocks/StartBottomLeftHorizontal.h"
#include "Blocks/StartTopLeftHorizontal.h"
#include "Blocks/StartBottomHorizontal.h"
#include "Blocks/StartAboveAndBelowLeft.h"

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

#if INSTRUMENTATION_ENABLED
#define CVIS_HEATMAPS true
#endif

namespace BuildingPlacement
{
    namespace
    {
        /* Used in e.g. mining tests to force us to use start blocks for all starting locations */
        bool useStartBlocksForAllStartingLocations = false;

        std::vector<Neighbourhood> ALL_NEIGHBOURHOODS = {Neighbourhood::MainBase, Neighbourhood::AllMyBases, Neighbourhood::HiddenBase};

        /* Stores a bitmask for each tile */
        /* 1: not buildable */
        /* 2: adjacent to not buildable */
        /* 4: reserved for block */
        /* 8: adjacent to reserved for block */
        std::vector<unsigned int> tileAvailability;

        std::shared_ptr<BuildBlock> startBlock;
        std::map<Base *, std::shared_ptr<BuildBlock>> baseToStartBlock;
        std::vector<std::shared_ptr<BuildBlock>> blocks;
        std::map<Base *, BaseStaticDefenseLocations> baseStaticDefenses;
        BaseStaticDefenseLocations emptyBaseStaticDefenses = {
                BWAPI::TilePositions::Invalid,
                std::vector<BWAPI::TilePosition>{},
                BWAPI::TilePositions::Invalid};

        std::unique_ptr<ForgeGatewayWall> forgeGatewayWall;

        std::shared_ptr<BuildBlock> chokeCannonBlock;
        BWAPI::TilePosition chokeCannonPlacement;

        bool updateRequired;
        std::map<Neighbourhood, std::set<const BWEM::Area *>> neighbourhoodAreas;
        std::map<const BWEM::Area *, BWAPI::Position> areaOrigins;
        std::map<const BWEM::Area *, BWAPI::Position> areaExits;
        BuildLocations availableBuildLocations;

        bool buildAwayFromExit;
        Base *hiddenBase;

        BuildLocationSet _availableGeysers;

        ForgeGatewayWall &getOrCreateForgeGatewayWall()
        {
            if (!forgeGatewayWall)
            {
                forgeGatewayWall = std::make_unique<ForgeGatewayWall>(createForgeGatewayWall(true, Map::getMyMain()));

                if (forgeGatewayWall->isValid())
                {
                    /* Remove any blocks that are overlapping the wall */
                    for (auto iterRef = blocks.begin(); iterRef != blocks.end(); )
                    {
                        /* Consider a buffer around the block */
                        auto cornerTL = (*iterRef)->cornerTL + BWAPI::TilePosition(-1, -1);
                        auto width = (*iterRef)->width() + 2;
                        auto height = (*iterRef)->height() + 2;

                        bool overlaps = false;
                        auto visit = [&](BWAPI::TilePosition tilePos, int x, int y)
                        {
                            overlaps = overlaps || Geom::Overlaps(tilePos, x, y, cornerTL, width, height);
                        };

                        visit(forgeGatewayWall->pylon, 2, 2);
                        visit(forgeGatewayWall->forge, 3, 2);
                        visit(forgeGatewayWall->gateway, 4, 3);
                        for (auto tilePos : forgeGatewayWall->cannons) visit(tilePos, 2, 2);
                        for (auto tilePos : forgeGatewayWall->naturalCannons) visit(tilePos, 2, 2);

                        if (overlaps)
                        {
                            iterRef = blocks.erase(iterRef);
                        } else {
                            iterRef++;
                        }
                    }

                    updateRequired = true;

                    if (!forgeGatewayWall->naturalCannons.empty())
                    {
                        auto natBase = Map::mapSpecificOverride()->naturalForWallPlacement(Map::getMyMain());
                        if (!natBase) natBase = Map::getMyNatural();
                        auto &naturalDefenses = baseStaticDefenses[natBase];
                        naturalDefenses.powerPylon = forgeGatewayWall->pylon;
                        naturalDefenses.workerDefenseCannons = forgeGatewayWall->naturalCannons;
                    }

#if CVIS_HEATMAPS
                    std::vector<long> wallHeatmap(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight(), 0);
                    forgeGatewayWall->addToHeatmap(wallHeatmap);
                    Telemetry::addHeatmap("Wall", wallHeatmap, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
#endif
                }
                else
                {
                    AppLog::Get() << "WARNING: No wall available";
                }
            }

            return *forgeGatewayWall;
        }

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

            for (auto baseLoc : Map::allBases())
            {
                for (int tileX = baseLoc->getTilePosition().x - 1; tileX <= baseLoc->getTilePosition().x + 4; tileX++)
                {
                    for (int tileY = baseLoc->getTilePosition().y - 1; tileY <= baseLoc->getTilePosition().y + 3; tileY++)
                    {
                        if (tileX < 0 || tileY < 0 || tileX >= BWAPI::Broodwar->mapWidth() || tileY >= BWAPI::Broodwar->mapHeight()) continue;

                        if (tileX == baseLoc->getTilePosition().x - 1 || tileY == baseLoc->getTilePosition().y - 1 || tileX == baseLoc->getTilePosition().x + 4
                            || tileY == baseLoc->getTilePosition().y + 3)
                        {
                            tileAvailability[tileX + tileY * BWAPI::Broodwar->mapWidth()] = 2;
                        }
                        else
                        {
                            tileAvailability[tileX + tileY * BWAPI::Broodwar->mapWidth()] = 1;
                        }
                    }
                }

                /* If the geyser is on the left, mark the top row and the row above it towards the nexus unbuildable */
                /* Building something here will interfere with gas collection */
                for (auto &geyserResource : baseLoc->geysersOrRefineries())
                {
                    auto geyserTile = geyserResource->tilePos;

                    if (geyserTile.x >= (baseLoc->getTilePosition().x - 2)) continue;
                    if (geyserTile.y < baseLoc->getTilePosition().y) continue;
                    if (geyserTile.y > (baseLoc->getTilePosition().y + 3)) continue;

                    tileAvailability[geyserTile.x + 4 + geyserTile.y * BWAPI::Broodwar->mapWidth()] = 1;
                    tileAvailability[geyserTile.x + 5 + geyserTile.y * BWAPI::Broodwar->mapWidth()] = 1;
                    tileAvailability[geyserTile.x + 6 + geyserTile.y * BWAPI::Broodwar->mapWidth()] = 1;
                    tileAvailability[geyserTile.x + 4 + (geyserTile.y - 1) * BWAPI::Broodwar->mapWidth()] = 1;
                    tileAvailability[geyserTile.x + 5 + (geyserTile.y - 1) * BWAPI::Broodwar->mapWidth()] = 1;
                    tileAvailability[geyserTile.x + 6 + (geyserTile.y - 1) * BWAPI::Broodwar->mapWidth()] = 1;
                }
            }
        }

        void updateNeighbourhoods()
        {
            neighbourhoodAreas.clear();
            areaOrigins.clear();
            areaExits.clear();

            /* Main base */
            neighbourhoodAreas[Neighbourhood::MainBase] = Map::getMyMainAreas();
            Map::mapSpecificOverride()->modifyMainBaseBuildingPlacementAreas(neighbourhoodAreas[Neighbourhood::MainBase]);

            auto mainChoke = Map::getMyMainChoke();
            auto mainExit = mainChoke ? mainChoke->midpoint : Map::getMyMain()->getPosition();
            auto origin = Map::getMyMain()->mineralLineCenter;
            for (const auto &area : neighbourhoodAreas[Neighbourhood::MainBase])
            {
                areaOrigins[area] = origin;
                areaExits[area] = mainExit;
            }

            /* All other owned bases */
            /* We use the main choke exit as the exit to promote building closer to our main */
            neighbourhoodAreas[Neighbourhood::AllMyBases] = neighbourhoodAreas[Neighbourhood::MainBase];
            for (const auto &baseLoc : Map::getMyBases())
            {
                if (baseLoc == Map::getMyMain()) continue;
                if (baseLoc->island) continue;

                if (!baseLoc->resourceDepot) continue;

                neighbourhoodAreas[Neighbourhood::AllMyBases].insert(baseLoc->getArea());
                areaOrigins[baseLoc->getArea()] = baseLoc->mineralLineCenter;
                areaExits[baseLoc->getArea()] = mainExit;
            }

            /* Hidden base */
            if (hiddenBase)
            {
                auto addArea = [](auto area)
                {
                    neighbourhoodAreas[Neighbourhood::HiddenBase].insert(area);
                    areaOrigins[area] = hiddenBase->mineralLineCenter;
                    areaExits[area] = hiddenBase->mineralLineCenter;
                };

                addArea(hiddenBase->getArea());

                /* Include nearby areas in case the base area itself has no building locations */
                for (auto &choke : hiddenBase->getArea()->ChokePoints())
                {
                    if (hiddenBase->getPosition().getApproxDistance(BWAPI::Position(choke->Center()) + BWAPI::Position(4, 4)) < 640)
                    {
                        addArea(choke->GetAreas().first);
                        addArea(choke->GetAreas().second);
                    }
                }
            }
        }

        void addBaseStaticDefense(Base *baseLoc,
                                  BWAPI::TilePosition powerPylon,
                                  std::vector<BWAPI::TilePosition> workerDefenseCannons,
                                  BWAPI::TilePosition startBlockCannon = BWAPI::TilePositions::Invalid)
        {
            auto markLocation = [](const BWAPI::TilePosition &tilePos)
            {
                for (int tileX = tilePos.x - 1; tileX <= tilePos.x + 2; tileX++)
                {
                    for (int tileY = tilePos.y - 1; tileY <= tilePos.y + 2; tileY++)
                    {
                        if (tileX < 0 || tileY < 0 || tileX >= BWAPI::Broodwar->mapWidth() || tileY >= BWAPI::Broodwar->mapHeight()) continue;

                        if (tileX == tilePos.x - 1 || tileY == tilePos.y - 1 || tileX == tilePos.x + 2 || tileY == tilePos.y + 2)
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

            markLocation(powerPylon);
            for (auto &cannon : workerDefenseCannons)
            {
                markLocation(cannon);
            }

            baseStaticDefenses.emplace(baseLoc, BaseStaticDefenseLocations{powerPylon, std::move(workerDefenseCannons), startBlockCannon});
        }

        void findStartBlock(Base *baseLoc)
        {
            std::vector<std::shared_ptr<BuildBlock>> startBlocks = {
                    std::make_shared<StartNormalLeft>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<StartNormalRight>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<StartCompactLeft>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<StartCompactRight>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<StartCompactLeftHorizontal>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<StartCompactRightVertical>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<StartBottomLeftHorizontal>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<StartTopLeftHorizontal>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<StartBottomHorizontal>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
                    std::make_shared<StartAboveAndBelowLeft>(BWAPI::TilePositions::Invalid, BWAPI::TilePositions::Invalid),
            };

            for (const auto &blockType : startBlocks)
            {
                auto block = blockType->tryCreate(baseLoc->getTilePosition(), tileAvailability);
                if (block)
                {
                    if (baseLoc == Map::getMyMain()) startBlock = block;
                    baseToStartBlock[baseLoc] = block;
                    blocks.push_back(block);

                    /* Check if this start block has a location for a defensive cannon */
                    BWAPI::TilePosition cannon = BWAPI::TilePositions::Invalid;
                    for (auto &location : block->small)
                    {
                        if (location.tilePos == block->powerPylon) continue;
                        cannon = location.tilePos;
                        break;
                    }

                    addBaseStaticDefense(baseLoc, block->powerPylon, block->cannons, cannon);
                    return;
                }
            }

            AppLog::Get() << "WARNING: No start block available for " << baseLoc->getTilePosition();
        }

        void findBaseStaticDefenses()
        {
            for (auto &baseLoc : Map::allBases())
            {
                /* Main has its locations handled by the start block */
                if (baseLoc == Map::getMyMain() && baseStaticDefenses.contains(baseLoc))
                {
                    auto &mainDefenses = baseStaticDefenses[baseLoc];

                    /* Start by scoring the positions by how well they defend the mineral line */
                    auto mainChoke = Map::getMyMainChoke();
                    auto chokeToMineralLineDist = mainChoke ? mainChoke->midpoint.getApproxDistance(baseLoc->mineralLineCenter) : 0;
                    std::vector<std::pair<int, BWAPI::TilePosition>> tilesAndScore;
                    for (auto &tilePos : mainDefenses.workerDefenseCannons)
                    {
                        auto pt = BWAPI::Position(tilePos) + BWAPI::Position(16, 16);
                        int score = pt.getApproxDistance(baseLoc->mineralLineCenter) * 4;
                        if (!baseLoc->geysersOrRefineries().empty())
                        {
                            score += pt.getApproxDistance((*baseLoc->geysersOrRefineries().begin())->midpoint);
                        }
                        if (mainChoke)
                        {
                            score += (mainChoke->midpoint.getApproxDistance(pt) - chokeToMineralLineDist) * 2;
                        }
                        tilesAndScore.emplace_back(score, tilePos);
                    }
                    std::sort(tilesAndScore.begin(), tilesAndScore.end());

                    /* Next find the position that best defends the start block */
                    auto startBlockPylonPos = BWAPI::Position(startBlock->powerPylon) + BWAPI::Position(16, 16);
                    auto chokeToStartBlockDist = mainChoke
                            ? mainChoke->midpoint.getApproxDistance(startBlockPylonPos)
                            : 0;
                    BWAPI::TilePosition bestStartBlockTile = BWAPI::TilePositions::Invalid;
                    int bestScore = INT_MAX;
                    for (auto &tilePos : mainDefenses.workerDefenseCannons)
                    {
                        auto pt = BWAPI::Position(tilePos) + BWAPI::Position(16, 16);
                        int score = pt.getApproxDistance(startBlockPylonPos);
                        if (mainChoke)
                        {
                            score += (mainChoke->midpoint.getApproxDistance(pt) - chokeToStartBlockDist);
                        }
                        if (score < bestScore)
                        {
                            bestStartBlockTile = tilePos;
                            bestScore = score;
                        }
                    }

                    /* Now assign the locations */
                    /* We use the scores for how well they defend the mineral line, but put the cannon that best defends the start block second */
                    mainDefenses.workerDefenseCannons.clear();
                    auto firstTile = tilesAndScore.begin()->second;
                    mainDefenses.workerDefenseCannons.push_back(firstTile);
                    if (bestStartBlockTile != BWAPI::TilePositions::Invalid && bestStartBlockTile != firstTile)
                    {
                        mainDefenses.workerDefenseCannons.push_back(bestStartBlockTile);
                    }
                    for (auto &tileAndScore : tilesAndScore)
                    {
                        if (tileAndScore.second == firstTile) continue;
                        if (tileAndScore.second == bestStartBlockTile) continue;
                        mainDefenses.workerDefenseCannons.push_back(tileAndScore.second);
                    }

                    continue;
                }

                auto analysis = initializeBaseDefenseAnalysis(baseLoc);
                auto end1 = std::get<0>(analysis);
                auto end2 = std::get<1>(analysis);
                auto positions = std::get<2>(analysis);
                if (!end1 || !end2 || positions.empty()) continue;

                auto usePosition = [&positions](BWAPI::TilePosition tilePos)
                {
                    for (int x = tilePos.x - 1; x <= tilePos.x + 1; x++)
                    {
                        for (int y = tilePos.y - 1; y <= tilePos.y + 1; y++)
                        {
                            positions.erase(BWAPI::TilePosition(x, y));
                        }
                    }
                };

                std::vector<BWAPI::TilePosition> cannons;

                /* Now place a cannon closest to each end */
                auto placeEnd = [&](const Resource &end)
                {
                    int minDist = INT_MAX;
                    BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
                    for (auto tilePos : positions)
                    {
                        int span = end->midpoint.getApproxDistance(BWAPI::Position(tilePos) + BWAPI::Position(16, 16));
                        if (span < minDist)
                        {
                            minDist = span;
                            best = tilePos;
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

                /* Now place the pylon so that it powers both and is as far as possible from minerals and geyser */
                BWAPI::TilePosition pylon = BWAPI::TilePositions::Invalid;
                {
                    int geyserX = 0;
                    int geyserY = 0;
                    int geyserCount = 0;
                    for (const auto &geyser : baseLoc->geysersOrRefineries())
                    {
                        geyserX += geyser->midpoint.x;
                        geyserY += geyser->midpoint.y;
                        geyserCount++;
                    }
                    auto geyserPos = geyserCount == 0
                                     ? BWAPI::Positions::Invalid
                                     : BWAPI::Position(geyserX / geyserCount, geyserY / geyserCount);

                    int maxDist = 0;
                    for (auto tilePos : positions)
                    {
                        /* First ensure the position powers the cannons */
                        bool powersAll = true;
                        for (auto cannon : cannons)
                        {
                            if (!UnitHelper::Powers(tilePos, cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon))
                            {
                                powersAll = false;
                                break;
                            }
                        }
                        if (!powersAll) continue;

                        /* Next check the distances */
                        auto pt = BWAPI::Position(tilePos) + BWAPI::Position(16, 16);
                        int span = pt.getApproxDistance(baseLoc->mineralLineCenter);
                        if (geyserCount > 0)
                        {
                            /* Weight minerals twice as high as geyser */
                            span *= 2;
                            span += pt.getApproxDistance(geyserPos);
                        }

                        if (span > maxDist)
                        {
                            maxDist = span;
                            pylon = tilePos;
                        }
                    }

                    if (!pylon.isValid()) continue;

                    usePosition(pylon);
                }

                /* TODO: Place additional cannons */

                addBaseStaticDefense(baseLoc, pylon, cannons);
            }
        }

        void findBlocks()
        {
            std::vector<std::shared_ptr<BuildBlock>> normalBlocks = {
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
                BWAPI::TilePosition midpoint = BWAPI::TilePosition(
                        (BWAPI::Broodwar->mapWidth() - blockType->width()) / 2,
                        (BWAPI::Broodwar->mapHeight() - blockType->height()) / 2);

                for (int tileX = 0; tileX <= midpoint.x; tileX++)
                {
                    for (int tileY = 0; tileY <= midpoint.y; tileY++)
                    {
                        if (auto block = blockType->tryCreate(BWAPI::TilePosition(tileX, tileY), tileAvailability)) blocks.push_back(block);
                    }
                }

                for (int tileX = BWAPI::Broodwar->mapWidth() - blockType->width(); tileX > midpoint.x; tileX--)
                {
                    for (int tileY = 0; tileY <= midpoint.y; tileY++)
                    {
                        if (auto block = blockType->tryCreate(BWAPI::TilePosition(tileX, tileY), tileAvailability)) blocks.push_back(block);
                    }
                }

                for (int tileX = 0; tileX <= midpoint.x; tileX++)
                {
                    for (int tileY = BWAPI::Broodwar->mapHeight() - blockType->height(); tileY > midpoint.y; tileY--)
                    {
                        if (auto block = blockType->tryCreate(BWAPI::TilePosition(tileX, tileY), tileAvailability)) blocks.push_back(block);
                    }
                }

                for (int tileX = BWAPI::Broodwar->mapWidth() - blockType->width(); tileX > midpoint.x; tileX--)
                {
                    for (int tileY = BWAPI::Broodwar->mapHeight() - blockType->height(); tileY > midpoint.y; tileY--)
                    {
                        if (auto block = blockType->tryCreate(BWAPI::TilePosition(tileX, tileY), tileAvailability)) blocks.push_back(block);
                    }
                }
            }
        }

        void findMainChokeCannonPlacement()
        {
            auto mainChoke = Map::getMyMainChoke();
            if (!mainChoke) return;

            /* Gather main base blocks */
            /* At the same time, find the closest small building location that has detection on the choke center */
            std::vector<std::shared_ptr<BuildBlock>> mainBlocks;
            auto closestBlockLocation = BWAPI::TilePositions::Invalid;
            std::shared_ptr<BuildBlock> closestBlockLocationBlock = nullptr;
            auto closestDist = INT_MAX;
            {
                for (auto &block : blocks)
                {
                    /* Ignore non-main-base */
                    if (!neighbourhoodAreas[Neighbourhood::MainBase].contains(
                            BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(block->midpoint()))))
                    {
                        continue;
                    }

                    mainBlocks.push_back(block);

                    /* Search for a close small location */
                    for (const auto &location : block->small)
                    {
                        if (location.tilePos == block->powerPylon) continue;
                        if (!UnitHelper::Powers(block->powerPylon, location.tilePos, BWAPI::UnitTypes::Protoss_Photon_Cannon)) continue;

                        int span = Geom::EdgeToPointDistance(BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                                            BWAPI::Position(location.tilePos) + BWAPI::Position(32, 32),
                                                            mainChoke->midpoint);
                        if (span < 3 * 32 || span > 8 * 32) continue;

                        if (span < closestDist)
                        {
                            closestDist = span;
                            closestBlockLocation = location.tilePos;
                            closestBlockLocationBlock = block;
                        }
                    }
                }
            }

            /* TODO: Check if there is a 2x2 block that fits? */

            /* Try to find a location for a cannon that fulfills the following conditions: */
            /* - Is in our main */
            /* - Is within detection range of the choke center */
            /* - Is powered by some block */
            /* - Only borders unbuildable or reserved tiles on one side, and in the latter case, leaves room on at least one side */
            auto closestLocation = BWAPI::TilePositions::Invalid;
            std::shared_ptr<BuildBlock> closestLocationBlock = nullptr;
            {
                auto isUnbuildableOrReserved = [](BWAPI::TilePosition tilePos)
                {
                    auto valueAt = [&](int offsetX, int offsetY)
                    {
                        int x = tilePos.x + offsetX;
                        if (x < 0 || x >= BWAPI::Broodwar->mapWidth()) return 1U;

                        int y = tilePos.y + offsetY;
                        if (y < 0 || y >= BWAPI::Broodwar->mapHeight()) return 1U;

                        if (!Map::isWalkable(x, y))
                        {
                            return tileAvailability[x + y * BWAPI::Broodwar->mapWidth()] | 16U;
                        }

                        return tileAvailability[x + y * BWAPI::Broodwar->mapWidth()];
                    };

                    /* Check for overlap with unbuildable or reserved for block */
                    if ((valueAt(0, 0) & 5U) != 0) return true;
                    if ((valueAt(1, 0) & 5U) != 0) return true;
                    if ((valueAt(0, 1) & 5U) != 0) return true;
                    if ((valueAt(1, 1) & 5U) != 0) return true;

                    /* Check all of the tiles around the cannon, counting how many times walkability or reserved for block changes */
                    /* Accept if it changes at most twice and has 3 or fewer reserved */
                    auto lastValue = valueAt(-1, -1) & 20U;
                    auto changes = 0U;
                    auto reserved = 0U;
                    auto visit = [&](int offsetX, int offsetY)
                    {
                        auto here = valueAt(offsetX, offsetY) & 20U;
                        if (here != lastValue)
                        {
                            lastValue = here;
                            changes++;
                        }
                        if ((here & 4U) != 0) reserved++;
                    };
                    visit(0, -1);
                    visit(1, -1);
                    visit(2, -1);
                    visit(2, 0);
                    visit(2, 1);
                    visit(2, 2);
                    visit(1, 2);
                    visit(0, 2);
                    visit(-1, 2);
                    visit(-1, 1);
                    visit(-1, 0);
                    visit(-1, -1);

                    return changes > 2 || reserved > 3;
                };

                auto chokeTile = BWAPI::TilePosition(mainChoke->midpoint);
                for (int x = chokeTile.x - 9; x <= chokeTile.x + 9; x++)
                {
                    for (int y = chokeTile.y - 9; y <= chokeTile.y + 9; y++)
                    {
                        BWAPI::TilePosition here(x, y);
                        if (!here.isValid()) continue;

                        /* Ensure it is in detection range of the choke center */
                        int span = Geom::EdgeToPointDistance(BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                                            BWAPI::Position(here) + BWAPI::Position(32, 32),
                                                            mainChoke->midpoint);
                        if (span < 3 * 32 || span > 7 * 32) continue;
                        if (span > closestDist) continue;

                        /* Ensure it is in the main area */
                        if (!neighbourhoodAreas[Neighbourhood::MainBase].contains(
                                BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(here) + BWAPI::WalkPosition(4, 4))))
                        {
                            continue;
                        }

                        /* Ensure it can fit here */
                        if (isUnbuildableOrReserved(here)) continue;

                        /* Find the furthest pylon that powers it */
                        int furthestPowerDist = 0;
                        std::shared_ptr<BuildBlock> furthestPowerBlock = nullptr;
                        for (const auto &block : mainBlocks)
                        {
                            if (!UnitHelper::Powers(block->powerPylon, here, BWAPI::UnitTypes::Protoss_Photon_Cannon)) continue;

                            int powerDist = BWAPI::Position(block->powerPylon).getApproxDistance(BWAPI::Position(here));
                            if (powerDist > furthestPowerDist)
                            {
                                furthestPowerDist = powerDist;
                                furthestPowerBlock = block;
                            }
                        }
                        if (!furthestPowerBlock) continue;

                        closestDist = span;
                        closestLocation = here;
                        closestLocationBlock = furthestPowerBlock;
                    }
                }
            }

            /* Use the best result */
            if (closestLocation.isValid())
            {
                chokeCannonPlacement = closestLocation;
                chokeCannonBlock = closestLocationBlock;
            }
            else if (closestBlockLocation.isValid())
            {
                closestBlockLocationBlock->tilesReserved(closestBlockLocation, BWAPI::UnitTypes::Protoss_Photon_Cannon.tileSize(), true);
                chokeCannonPlacement = closestBlockLocation;
                chokeCannonBlock = closestBlockLocationBlock;
                if (closestDist > 7 * 32)
                {
                    AppLog::Get() << "WARNING: Best choke cannon placement is a bit too far away";
                }
            }
            else
            {
                AppLog::Get() << "WARNING: No choke cannon placement";
            }
        }

        int distanceToExit(Neighbourhood location, BWAPI::Position exit, BWAPI::TilePosition tilePos, BWAPI::UnitType type)
        {
            auto span = Pathfinder::GetGroundDistance(
                    BWAPI::Position(tilePos) + (BWAPI::Position(type.tileSize()) / 2),
                    exit,
                    BWAPI::UnitTypes::Protoss_Dragoon,
                    Pathfinder::PathFindingOptions::UseNearestBWEMArea);

            /* For the main base neighbourhood, prefer not to place large buildings too close to the exit */
            /* Rationale: the main base exit is probably a choke and we don't want to have our buildings */
            /* get in the way of our choke defense */
            if (type.tileWidth() == 4 && location == Neighbourhood::MainBase && span < 320)
            {
                span = 320 + (320 - span);
            }

            return span;
        }

        /* At how many frames from now will the build position given by the tile and type be powered */
        /* If the position is already powered, returns 0 */
        /* If the position will be powered by a pending pylon, returns the number of frames until the pylon is complete */
        /* Otherwise returns -1 */
        int poweredAfter(BWAPI::TilePosition tilePos, BWAPI::UnitType type, std::vector<Building *> &pendingPylons)
        {
            if (BWAPI::Broodwar->hasPower(tilePos, type)) return 0;

            int outcome = -1;
            for (auto &pendingPylon : pendingPylons)
            {
                if (UnitHelper::Powers(pendingPylon->tilePos, tilePos, type) &&
                    (outcome == -1 || pendingPylon->expectedFramesUntilCompletion() < outcome))
                {
                    outcome = pendingPylon->expectedFramesUntilCompletion();
                }
            }

            return outcome;
        }

        /* Rebuilds the map of available build locations */
        void updateAvailableBuildLocations()
        {
            BuildLocations outcome;

            /* Gather our pending pylons */
            auto pendingPylons = Constructor::pendingBuildingsOfType(BWAPI::UnitTypes::Protoss_Pylon);

            /* Scan blocks to: */
            /* - Collect the powered (or soon-to-be-powered) medium and large build locations we have available */
            /* - Collect the next pylon to be built in each block */
            for (auto &block : blocks)
            {
                /* Consider medium building positions */
                std::vector<std::tuple<BuildBlock::Location, int>> poweredMedium;
                std::vector<BuildBlock::Location> unpoweredMedium;
                for (auto placement : block->medium)
                {
                    int framesToPower = poweredAfter(placement.tilePos, BWAPI::UnitTypes::Protoss_Forge, pendingPylons);
                    if (framesToPower == -1)
                    {
                        unpoweredMedium.push_back(placement);
                    }
                    else
                    {
                        poweredMedium.emplace_back(placement, framesToPower);
                    }
                }

                /* Consider large building positions */
                std::vector<std::tuple<BuildBlock::Location, int>> poweredLarge;
                std::vector<BuildBlock::Location> unpoweredLarge;
                for (auto placement : block->large)
                {
                    int framesToPower = poweredAfter(placement.tilePos, BWAPI::UnitTypes::Protoss_Gateway, pendingPylons);
                    if (framesToPower == -1)
                    {
                        unpoweredLarge.push_back(placement);
                    }
                    else
                    {
                        poweredLarge.emplace_back(placement, framesToPower);
                    }
                }

                /* If the block is full, continue now */
                if (block->small.empty() && poweredMedium.empty() && poweredLarge.empty())
                {
                    continue;
                }

                /* Add data from the block to appropriate neighbourhoods */
                for (auto &neighbourhood : ALL_NEIGHBOURHOODS)
                {
                    /* Make sure we don't die if we for some reason have an unconfigured neighbourhood */
                    if (!neighbourhoodAreas.contains(neighbourhood)) continue;

                    auto area = BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(block->midpoint()));

                    /* Check if this block fits in this neighbourhood */
                    if (!neighbourhoodAreas[neighbourhood].contains(BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(block->midpoint()))))
                    {
                        continue;
                    }

                    /* Get the origin and exit */
                    BWAPI::Position origin, exit;
                    {
                        auto iterRef = areaOrigins.find(area);
                        if (iterRef == areaOrigins.end()) continue;
                        origin = iterRef->second;
                    }
                    {
                        auto iterRef = areaExits.find(area);
                        if (iterRef == areaExits.end())
                        {
                            exit = origin;
                        }
                        else
                        {
                            exit = iterRef->second;
                        }
                    }

                    /* Add pylons */
                    for (auto pylonLocation : block->small)
                    {
                        BuildLocation pylon(pylonLocation,
                                            builderFrames(origin, block->small.begin()->tilePos, BWAPI::UnitTypes::Protoss_Pylon),
                                            0,
                                            distanceToExit(neighbourhood, exit, block->small.begin()->tilePos, BWAPI::UnitTypes::Protoss_Pylon));

                        if (pylonLocation.tilePos == block->powerPylon)
                        {
                            for (auto &location : unpoweredMedium)
                            {
                                pylon.powersMedium.emplace_back(
                                        location,
                                        builderFrames(origin, location.tilePos, BWAPI::UnitTypes::Protoss_Forge),
                                        0,
                                        distanceToExit(neighbourhood, exit, location.tilePos, BWAPI::UnitTypes::Protoss_Forge),
                                        true);
                            }
                            for (auto &location : unpoweredLarge)
                            {
                                pylon.powersLarge.emplace_back(
                                        location,
                                        builderFrames(origin, location.tilePos, BWAPI::UnitTypes::Protoss_Gateway),
                                        0,
                                        distanceToExit(neighbourhood, exit, location.tilePos, BWAPI::UnitTypes::Protoss_Gateway));
                            }
                        }

                        outcome[to_underlying(neighbourhood)][2].emplace_back(pylon);
                    }

                    for (auto &tileAndPoweredAt : poweredMedium)
                    {
                        outcome[to_underlying(neighbourhood)][3].emplace_back(
                                std::get<0>(tileAndPoweredAt),
                                builderFrames(origin, std::get<0>(tileAndPoweredAt).tilePos, BWAPI::UnitTypes::Protoss_Forge),
                                std::get<1>(tileAndPoweredAt),
                                distanceToExit(neighbourhood, exit, std::get<0>(tileAndPoweredAt).tilePos, BWAPI::UnitTypes::Protoss_Forge),
                                true);
                    }

                    for (auto &tileAndPoweredAt : poweredLarge)
                    {
                        outcome[to_underlying(neighbourhood)][4].emplace_back(
                                std::get<0>(tileAndPoweredAt),
                                builderFrames(origin, std::get<0>(tileAndPoweredAt).tilePos, BWAPI::UnitTypes::Protoss_Gateway),
                                std::get<1>(tileAndPoweredAt),
                                distanceToExit(neighbourhood, exit, std::get<0>(tileAndPoweredAt).tilePos, BWAPI::UnitTypes::Protoss_Gateway));
                    }
                }
            }

            for (int neighbourhoodIdx=0; neighbourhoodIdx<NEIGHBOURHOOD_COUNT; neighbourhoodIdx++)
            {
                for (int size = 2; size <= 4; size++)
                {
                    std::sort(outcome[neighbourhoodIdx][size].begin(), outcome[neighbourhoodIdx][size].end(), BuildLocationCmp());
                }
            }

            availableBuildLocations = outcome;
        }

        void updateFramesUntilPowered()
        {
            /* Gather our pending pylons */
            auto pendingPylons = Constructor::pendingBuildingsOfType(BWAPI::UnitTypes::Protoss_Pylon);

            /* Loop and update every location with a current framesUntilPowered value */
            for (auto &neighbourhoodLocations : availableBuildLocations)
            {
                for (int size = 3; size <= 4; size++)
                {
                    auto &locations = neighbourhoodLocations[size];

                    bool anythingUpdated = false;
                    for (auto &location : locations)
                    {
                        if (location.framesUntilPowered > 0)
                        {
                            anythingUpdated = true;
                            location.framesUntilPowered = poweredAfter(
                                    location.location.tilePos,
                                    size == 3 ? BWAPI::UnitTypes::Protoss_Forge : BWAPI::UnitTypes::Protoss_Gateway,
                                    pendingPylons);
                        }
                    }

                    if (anythingUpdated)
                    {
                        std::sort(locations.begin(), locations.end(), BuildLocationCmp());
                    }
                }
            }
        }

        void updateAvailableGeysers()
        {
            _availableGeysers.clear();

            for (auto &baseLoc : Map::allBases())
            {
                if (baseLoc->owner != BWAPI::Broodwar->self()) continue;
                if (!baseLoc->resourceDepot) continue;
                if (!baseLoc->resourceDepot->completed &&
                    (baseLoc->resourceDepot->estimatedCompletionFrame - frameNow)
                    > UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Assimilator))
                {
                    continue;
                }

                for (const auto &geyser : baseLoc->geysersOrRefineries())
                {
                    if (geyser->refinery) continue;
                    if (Constructor::isPendingHere(geyser->tilePos)) continue;

                    /* TODO: Order in some logical way */
                    _availableGeysers.emplace_back(BuildBlock::Location(geyser->tilePos), 0, 0, 0);
                }
            }
        }

        void dumpHeatmap()
        {
#if CVIS_HEATMAPS
            /* We dump a heatmap with the following values: */
            /* - No building: 0 */
            /* - Large building: 2 */
            /* - Medium building: 3 */
            /* - Pylon: 4 */
            /* - Defensive location: 5 (first 8) */
            /* - Choke cannon placement: 10 */

            std::vector<long> blocksHeatmap(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight(), 0);

            auto addLocation = [&blocksHeatmap](BWAPI::TilePosition tilePos, int width, int height, int value)
            {
                auto bottomRight = tilePos + BWAPI::TilePosition(width, height);

                for (int y = tilePos.y; y < bottomRight.y; y++)
                {
                    if (y > BWAPI::Broodwar->mapHeight() - 1)
                    {
                        AppLog::Get() << "ERROR: BUILD LOCATION OUT OF BOUNDS @ " << tilePos;
                        continue;
                    }

                    for (int x = tilePos.x; x < bottomRight.x; x++)
                    {
                        if (x > BWAPI::Broodwar->mapWidth() - 1)
                        {
                            AppLog::Get() << "ERROR: BUILD LOCATION OUT OF BOUNDS @ " << tilePos;
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
                    if (!placement.converted) addLocation(placement.tilePos, 4, 3, 2);
                }
                for (auto placement : block->medium)
                {
                    if (!placement.converted) addLocation(placement.tilePos, 3, 2, 3);
                }
                for (auto placement : block->small)
                {
                    if (!placement.converted) addLocation(placement.tilePos, 2, 2, 4);
                }
            }

            for (const auto &[baseLoc, staticDefenses] : baseStaticDefenses)
            {
                addLocation(staticDefenses.powerPylon, 2, 2, 4);
                bool first = true;
                for (const auto &cannon : staticDefenses.workerDefenseCannons)
                {
                    addLocation(cannon, 2, 2, first ? 8 : 5);
                    first = false;
                }
            }

            if (chokeCannonPlacement.isValid())
            {
                addLocation(chokeCannonPlacement, 2, 2, 10);
            }

            Telemetry::addHeatmap("Blocks", blocksHeatmap, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
#endif
        }
    }

    void initialize()
    {
        neighbourhoodAreas.clear();
        areaOrigins.clear();
        areaExits.clear();
        tileAvailability.clear();
        startBlock = nullptr;
        baseToStartBlock.clear();
        blocks.clear();
        baseStaticDefenses.clear();
        chokeCannonBlock = nullptr;
        chokeCannonPlacement = BWAPI::TilePositions::Invalid;
        updateRequired = true;
        _availableGeysers.clear();
        buildAwayFromExit = false;
        hiddenBase = nullptr;
        forgeGatewayWall = nullptr;

        initializeTileAvailability();
        updateNeighbourhoods();
        if (useStartBlocksForAllStartingLocations)
        {
            for (auto &baseLoc : Map::allStartingLocations())
            {
                findStartBlock(baseLoc);
            }
        }
        else
        {
            findStartBlock(Map::getMyMain());
        }
        findBaseStaticDefenses();
        findBlocks();
        findMainChokeCannonPlacement();

        if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg) getOrCreateForgeGatewayWall();

        dumpHeatmap();
    }

    void onBuildingQueued(const Building *building)
    {
        for (auto &block : blocks)
        {
            updateRequired = block->tilesReserved(building->tilePos, building->type.tileSize()) || updateRequired;
        }
    }

    void onBuildingCancelled(const Building *building)
    {
        for (auto &block : blocks)
        {
            updateRequired = block->tilesFreed(building->tilePos, building->type.tileSize()) || updateRequired;
        }
    }

    void onUnitCreate(const Unit &gameUnit)
    {
        if (!gameUnit->type.isBuilding()) return;

        for (auto &block : blocks)
        {
            updateRequired = block->tilesUsed(gameUnit->getTilePosition(), gameUnit->type.tileSize()) || updateRequired;
        }

        /* Creation of depots indicates we've taken a new base */
        updateRequired = (gameUnit->player == BWAPI::Broodwar->self() && gameUnit->type.isResourceDepot()) || updateRequired;
    }

    void onUnitDestroy(const Unit &gameUnit)
    {
        if (!gameUnit->type.isBuilding()) return;

        for (auto &block : blocks)
        {
            updateRequired = block->tilesFreed(gameUnit->getTilePosition(), gameUnit->type.tileSize()) || updateRequired;
        }

        /* Destruction of depots indicates we've lost a base */
        updateRequired = updateRequired || (gameUnit->player == BWAPI::Broodwar->self() && gameUnit->type.isResourceDepot());
    }

    void onMainChokeChanged()
    {
        findMainChokeCannonPlacement();
        updateRequired = true;
    }

    void update()
    {
        /* Build away from the exit if the enemy has units in our base or is doing a rush */
        bool newBuildAwayFromExit = Director::getStrategyEngine()->isEnemyRushing()
                                    || !UnitMgr::enemyAtBase(Map::getMyMain()).empty();
        if (newBuildAwayFromExit != buildAwayFromExit)
        {
            buildAwayFromExit = newBuildAwayFromExit;
            updateRequired = true;
        }

        if (!hiddenBase)
        {
            hiddenBase = Map::getHiddenBase();
            if (hiddenBase) updateRequired = true;
        }

        if (updateRequired)
        {
            updateNeighbourhoods();
            updateAvailableBuildLocations();
            updateRequired = false;
        }
        else
        {
            /* We still need to update framesUntilPowered each frame */
            updateFramesUntilPowered();
        }

        updateAvailableGeysers();
    }

    BuildLocations &getBuildLocations()
    {
        return availableBuildLocations;
    }

    BuildLocationSet &availableGeysers()
    {
        return _availableGeysers;
    }

    bool BuildLocationCmp::operator()(const BuildLocation &a, const BuildLocation &b) const
    {
        /* Always sort the start block pylon before everything else */
        if (startBlock && startBlock->powerPylon == a.location.tilePos) return true;
        if (startBlock && startBlock->powerPylon == b.location.tilePos) return false;

        /* If our enemy is Protoss, prioritize the cannon choke power pylon next */
        /* We use this for detection on our choke in reaction to DTs */
        if (chokeCannonBlock && BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss && !buildAwayFromExit)
        {
            if (chokeCannonBlock->powerPylon == a.location.tilePos) return true;
            if (chokeCannonBlock->powerPylon == b.location.tilePos) return false;
        }

        /* Prioritize main geyser first */
        if (Map::getMyMain()->hasGeyserOrRefineryAt(a.location.tilePos)) return true;
        if (Map::getMyMain()->hasGeyserOrRefineryAt(b.location.tilePos)) return false;

        /* Prioritize locations that will be powered first */
        if (a.framesUntilPowered < b.framesUntilPowered) return true;
        if (a.framesUntilPowered > b.framesUntilPowered) return false;

        /* For medium locations, prioritize locations without an exit first - the producer will keep looking if it needs to place a building */
        /* with an exit */
        if (!a.location.hasExit && b.location.hasExit) return true;
        if (a.location.hasExit && !b.location.hasExit) return false;

        /* Then prioritize locations that have not been converted from a larger position */
        if (!a.location.converted && b.location.converted) return true;
        if (a.location.converted && !b.location.converted) return false;

        /* Finally prioritize based on distance */
        /* We weight builder distance more than distance from exit */
        /* Tech buildings prefer further away from the exit */
        int aDist = (a.isTech || buildAwayFromExit) ? (a.builderFrames * 2 - a.distanceToExit) : (a.builderFrames * 2 + a.distanceToExit);
        int bDist = (b.isTech || buildAwayFromExit) ? (b.builderFrames * 2 - b.distanceToExit) : (b.builderFrames * 2 + b.distanceToExit);
        if (aDist < bDist) return true;
        if (aDist > bDist) return false;

        /* Default case if everything else is equal */
        return a.location.tilePos < b.location.tilePos;
    }

    /* Approximately how many frames it will take a builder to reach the given build position */
    /* TODO: Update location origins depending on whether there are workers at a base */
    int builderFrames(BWAPI::Position origin, BWAPI::TilePosition tilePos, BWAPI::UnitType type)
    {
        return Pathfinder::ExpectedTravelTime(
                origin,
                BWAPI::Position(tilePos) + (BWAPI::Position(type.tileSize()) / 2),
                BWAPI::Broodwar->self()->getRace().getWorker(),
                Pathfinder::PathFindingOptions::UseNearestBWEMArea);
    }

    BaseStaticDefenseLocations &baseStaticDefenseLocations(Base *baseLoc)
    {
        auto iterRef = baseStaticDefenses.find(baseLoc);
        return iterRef == baseStaticDefenses.end() ? emptyBaseStaticDefenses : iterRef->second;
    }

    BWAPI::TilePosition mainBlockStaticDefenseLocation()
    {
        if (!startBlock) return BWAPI::TilePositions::Invalid;

        for (auto &location : startBlock->small)
        {
            if (location.tilePos == startBlock->powerPylon) continue;

            return location.tilePos;
        }

        return BWAPI::TilePositions::Invalid;
    }

    std::pair<BWAPI::TilePosition, BWAPI::TilePosition> mainChokeCannonLocations()
    {
        return std::make_pair(chokeCannonBlock ? chokeCannonBlock->powerPylon : BWAPI::TilePositions::Invalid, chokeCannonPlacement);
    }

    bool isInNeighbourhood(BWAPI::TilePosition buildTile, Neighbourhood neighbourhood)
    {
        if (!buildTile.isValid()) return false;

        /* Return true when we don't know where a neighbourhood is */
        /* Otherwise we might lock up completely if we ask for a location in an uninitialized neighbourhood */
        if (!neighbourhoodAreas.contains(neighbourhood)) return true;

        /* Find the block containing the build tile */
        for (auto &block : blocks)
        {
            if (buildTile.x < block->cornerTL.x) continue;
            if (buildTile.x >= (block->cornerTL.x + block->width())) continue;
            if (buildTile.y < block->cornerTL.y) continue;
            if (buildTile.y >= (block->cornerTL.y + block->height())) continue;

            auto area = BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(block->midpoint()));
            return neighbourhoodAreas[neighbourhood].contains(area);
        }

        /* Check if the tile is a nexus */
        for (const auto &baseLoc : Map::allBases())
        {
            if (buildTile != baseLoc->getTilePosition()) continue;

            switch (neighbourhood)
            {
                case Neighbourhood::MainBase:
                    return baseLoc == Map::getMyMain();
                case Neighbourhood::AllMyBases:
                    return true;
                case Neighbourhood::HiddenBase:
                    return baseLoc == Map::getHiddenBase();
            }
        }

        AppLog::Get() << "WARNING: Tile " << buildTile << " not in a block";
        return false;
    }

    std::tuple<Resource, Resource, std::set<BWAPI::TilePosition>> initializeBaseDefenseAnalysis(Base *baseLoc)
    {
        /* Find the end mineral patches, which are the patches furthest away from each other */
        Resource end1, end2;
        {
            int maxDist = 0;
            auto &patches = baseLoc->mineralPatches();
            for (const auto &first : patches)
            {
                for (const auto &second : patches)
                {
                    int span = first->getDistance(second);
                    if (span > maxDist)
                    {
                        maxDist = span;
                        end1 = first;
                        end2 = second;
                    }
                }
            }

            /* If for whatever reason this base has no mineral patches, continue */
            if (maxDist == 0) return {};
        }

        std::set<BWAPI::TilePosition> positions;
        auto addPositionIfValid = [&positions](BWAPI::TilePosition cornerTL)
        {
            for (int y = cornerTL.y; y < cornerTL.y + 2; y++)
            {
                for (int x = cornerTL.x; x < cornerTL.x + 2; x++)
                {
                    if (!BWAPI::TilePosition(x, y).isValid()) return;
                    if ((tileAvailability[x + y * BWAPI::Broodwar->mapWidth()] & 1U) == 1) return;
                }
            }

            positions.insert(cornerTL);
        };
        for (int x = -2; x <= 4; x++)
        {
            addPositionIfValid(baseLoc->getTilePosition() + BWAPI::TilePosition(x, -2));
            addPositionIfValid(baseLoc->getTilePosition() + BWAPI::TilePosition(x, 3));
        }
        for (int y = -1; y <= 2; y++)
        {
            addPositionIfValid(baseLoc->getTilePosition() + BWAPI::TilePosition(-2, y));
            addPositionIfValid(baseLoc->getTilePosition() + BWAPI::TilePosition(4, y));
        }

        return std::make_tuple(end1, end2, positions);
    }

    bool hasForgeGatewayWall()
    {
        return getOrCreateForgeGatewayWall().isValid();
    }

    ForgeGatewayWall &getForgeGatewayWall()
    {
        return getOrCreateForgeGatewayWall();
    }

    void setUseStartBlocksForAllStartingLocations(bool value)
    {
        useStartBlocksForAllStartingLocations = value;
    }

    std::shared_ptr<BuildBlock> startBlockForBase(Base *baseLoc)
    {
        auto iterRef = baseToStartBlock.find(baseLoc);
        if (iterRef == baseToStartBlock.end()) return nullptr;
        return iterRef->second;
    }
}
