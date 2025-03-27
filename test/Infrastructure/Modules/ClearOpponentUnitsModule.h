#pragma once

#include "DoNothingModule.h"
#include "Geo.h"
#include "include/BaseFinder.h"

class ClearOpponentUnitsModule : public DoNothingModule
{
public:
    explicit ClearOpponentUnitsModule(bool randomizeOrderProcessTimer = false) : randomizeOrderProcessTimer(randomizeOrderProcessTimer) {}

    void onFrame() override
    {
        // At startup, kill all workers, lift the depot and move it to (0,0)
        if (BWAPI::Broodwar->getFrameCount() < 50)
        {
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType().isWorker())
                {
                    BWAPI::Broodwar->killUnit(unit);
                }
                if (unit->getType().isResourceDepot())
                {
                    if (!unit->isLifted())
                    {
                        unit->lift();
                    }
                    else if (unit->getOrder() != BWAPI::Orders::Move)
                    {
                        unit->move(BWAPI::Position(0, 0));
                    }
                }
            }
        }

        // Keep the depot alive
        if (BWAPI::Broodwar->getFrameCount() % 50 == 0)
        {
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType().isResourceDepot() && unit->getHitPoints() < 200)
                {
                    unit->setHitPoints(1500);
                }
            }
        }

        // Logic for randomizing the order process timer
        if (randomizeOrderProcessTimer)
        {
            if (BWAPI::Broodwar->getFrameCount() > 0 && bunkerPositions.size() < 2) return;

            // Generate bunker positions and marine positions at startup
            if (BWAPI::Broodwar->getFrameCount() == 0)
            {
                auto mapWidth = BWAPI::Broodwar->mapWidth();
                auto mapHeight = BWAPI::Broodwar->mapHeight();

                std::vector<bool> tileAvailable;
                tileAvailable.resize(mapWidth * mapHeight);

                // Initialize with buildability and walkability
                for (int tileX = 0; tileX < mapWidth; tileX++)
                {
                    for (int tileY = 0; tileY < mapHeight; tileY++)
                    {
                        if (!BWAPI::Broodwar->isBuildable(tileX, tileY))
                        {
                            tileAvailable[tileX + tileY * mapWidth] = false;
                            continue;
                        }

                        bool walkable = true;
                        for (int walkX = 0; walkX < 4; walkX++)
                        {
                            for (int walkY = 0; walkY < 4; walkY++)
                            {
                                if (!BWAPI::Broodwar->isWalkable((tileX << 2U) + walkX, (tileY << 2U) + walkY))
                                {
                                    walkable = false;
                                    goto breakInnerLoop;
                                }
                            }
                        }
                        breakInnerLoop:

                        tileAvailable[tileX + tileY * mapWidth] = walkable;
                    }
                }

                // Remove anything near any bases
                BaseFinder::Init();
                auto &bases = BaseFinder::GetBases();
                for (auto &base : bases)
                {
                    for (auto tileY = base.tpos.y - 10; tileY < base.tpos.y + 13; tileY++)
                    {
                        if (tileY < 0) continue;
                        if (tileY >= mapHeight) continue;

                        for (auto tileX = base.tpos.x - 10; tileX < base.tpos.x + 14; tileX++)
                        {
                            if (tileX < 0) continue;
                            if (tileX >= mapHeight) continue;

                            tileAvailable[tileX + tileY * mapWidth] = false;
                        }
                    }
                }

                // Find two bunker positions furthest from any base
                for (int i = 0; i < 2; i++)
                {
                    int bestDist = 0;
                    BWAPI::TilePosition bestTile = BWAPI::TilePositions::Invalid;
                    for (int tileX = 0; tileX < mapWidth; tileX++)
                    {
                        for (int tileY = 0; tileY < mapHeight; tileY++)
                        {
                            bool usable = true;
                            for (int dx = 0; usable && dx < 3; dx++)
                            {
                                for (int dy = 0; usable && dy < 3; dy++)
                                {
                                    if (!tileAvailable[(tileX + dx) + (tileY + dy) * mapWidth])
                                    {
                                        usable = false;
                                    }
                                }
                            }
                            if (!usable) continue;

                            BWAPI::Position center = BWAPI::Position(tileX, tileY) + BWAPI::Position(48, 32);
                            int minDist = INT_MAX;
                            for (auto &base : bases)
                            {
                                minDist = std::min(minDist, base.pos.getApproxDistance(center));
                            }

                            if (minDist > bestDist)
                            {
                                bestDist = minDist;
                                bestTile = BWAPI::TilePosition(tileX, tileY);
                            }
                        }
                    }

                    if (bestTile == BWAPI::TilePositions::Invalid) break;

                    bunkerPositions.emplace_back(bestTile);
                    for (int j = 0; j < 4; j++)
                    {
                        marinePositions.emplace_back(bestTile.x * 32 + 24 * j, (bestTile.y + 2) * 32);
                    }

                    for (int dx = 0; dx < 3; dx++)
                    {
                        for (int dy = 0; dy < 3; dy++)
                        {
                            tileAvailable[(bestTile.x + dx) + (bestTile.y + dy) * mapWidth] = false;
                        }
                    }
                }

                if (bunkerPositions.size() < 2)
                {
                    std::cout << "ERROR: Could not find two bunker positions" << std::endl;
                }
                std::string sep;
                std::cout << "Bunker positions: ";
                for (auto pos : bunkerPositions)
                {
                    std::cout << sep << pos;
                    sep = ", ";
                }
                std::cout << std::endl;
            }
            else if (BWAPI::Broodwar->getFrameCount() == 1)
            {
                for (auto &bunkerPos : bunkerPositions)
                {
                    BWAPI::Position center = BWAPI::Position(bunkerPos) + BWAPI::Position(48, 32);
                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, center);
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() == 7)
            {
                for (auto &bunkerPos : bunkerPositions)
                {
                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                BWAPI::UnitTypes::Terran_Bunker,
                                                Geo::CenterOfUnit(bunkerPos, BWAPI::UnitTypes::Terran_Bunker));
                }
                for (auto &unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer) BWAPI::Broodwar->killUnit(unit);
                }
            }
        }
    }

private:
    bool randomizeOrderProcessTimer = false;
    std::vector<BWAPI::TilePosition> bunkerPositions;
    std::vector<BWAPI::Position> marinePositions;
    std::map<BWAPI::Unit, std::vector<BWAPI::Unit>> bunkersAndUnits;
};