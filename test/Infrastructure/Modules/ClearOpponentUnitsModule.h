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
            if (BWAPI::Broodwar->getFrameCount() > 0 && bunkerAndMarinePositions.size() < 7) return;

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
                            if (tileX >= mapWidth) continue;

                            tileAvailable[tileX + tileY * mapWidth] = false;
                        }
                    }
                }

                // Remove anything overlapping neutrals
                for (auto &unit : BWAPI::Broodwar->neutral()->getUnits())
                {
                    auto topLeft = BWAPI::TilePosition(
                            unit->getPosition() + BWAPI::Position(-unit->getType().dimensionLeft(), -unit->getType().dimensionUp()));
                    auto bottomRight = BWAPI::TilePosition(
                            unit->getPosition() + BWAPI::Position(unit->getType().dimensionRight(), unit->getType().dimensionDown()));
                    for (int tileX = topLeft.x; tileX <= bottomRight.x; tileX++)
                    {
                        for (int tileY = topLeft.y; tileY <= bottomRight.y; tileY++)
                        {
                            tileAvailable[tileX + tileY * mapWidth] = false;
                        }
                    }
                }

                // Find seven bunker positions furthest from any base
                for (int i = 0; i < 7; i++)
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
                                for (auto &patch : base.minerals)
                                {
                                    minDist = std::min(minDist, patch->getDistance(center));
                                }
                                for (auto &geyser : base.geysers)
                                {
                                    minDist = std::min(minDist, geyser->getDistance(center));
                                }
                            }

                            if (minDist > bestDist)
                            {
                                bestDist = minDist;
                                bestTile = BWAPI::TilePosition(tileX, tileY);
                            }
                        }
                    }

                    if (bestTile == BWAPI::TilePositions::Invalid) break;

                    bunkerAndMarinePositions.emplace_back(
                            Geo::CenterOfUnit(BWAPI::Position(bestTile), BWAPI::UnitTypes::Terran_Bunker),
                            Geo::CenterOfUnit(BWAPI::Position(bestTile) + BWAPI::Position(0, 64), BWAPI::UnitTypes::Terran_Marine));

                    for (int dx = 0; dx < 3; dx++)
                    {
                        for (int dy = 0; dy < 3; dy++)
                        {
                            tileAvailable[(bestTile.x + dx) + (bestTile.y + dy) * mapWidth] = false;
                        }
                    }
                }

                if (bunkerAndMarinePositions.size() < 7)
                {
                    std::cout << "ERROR: Could not find seven bunker positions" << std::endl;
                }

                for (auto &[bunkerPos, _] : bunkerAndMarinePositions)
                {
                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, bunkerPos);
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() == 20)
            {
                for (auto &[bunkerPos, marinePos] : bunkerAndMarinePositions)
                {
                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Terran_Bunker, bunkerPos);
                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Terran_Marine, marinePos);
                }
                for (auto &unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer) BWAPI::Broodwar->killUnit(unit);
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() == 30)
            {
                std::map<BWAPI::Position, BWAPI::Unit> posToUnit;
                for (auto &unit : BWAPI::Broodwar->self()->getUnits())
                {
                    posToUnit[unit->getPosition()] = unit;
                }

                for (auto &[bunkerPos, marinePos] : bunkerAndMarinePositions)
                {
                    auto bunker = posToUnit[bunkerPos];
                    if (!bunker)
                    {
                        std::cout << "ERROR: Could not find bunker unit @ " << bunkerPos << std::endl;
                        continue;
                    }

                    auto marine = posToUnit[marinePos];
                    if (!marine)
                    {
                        std::cout << "ERROR: Marine not found @ " << marinePos << std::endl;
                        break;
                    }

                    bunkersAndMarines.emplace_back(bunker, marine);
                    bunker->load(marine);
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() % 150 == 0)
            {
                // Pop out between 0 and 7 marines before each order process timer reset
                int remainingMarines = nextMarineCount;
                for (auto &[bunker, marine] : bunkersAndMarines)
                {
                    if (remainingMarines == 0) break;
                    bunker->unload(marine);
                    remainingMarines--;
                }

                if (nextMarineCount == 7)
                {
                    nextMarineCount = 0;
                }
                else
                {
                    nextMarineCount++;
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() % 150 == 10)
            {
                // Load all of the unloaded marines
                for (auto &[bunker, marine] : bunkersAndMarines)
                {
                    bunker->load(marine);
                }
            }
        }
    }

private:
    bool randomizeOrderProcessTimer = false;
    std::vector<std::pair<BWAPI::Position, BWAPI::Position>> bunkerAndMarinePositions;
    std::vector<std::pair<BWAPI::Unit, BWAPI::Unit>> bunkersAndMarines;
    int nextMarineCount = 0;
};