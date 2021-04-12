#include "Squad.h"

#include "Map.h"
#include "Geo.h"
#include "Boids.h"
#include "Units.h"
#include "Players.h"
#include "NoGoAreas.h"

#include "DebugFlag_UnitOrders.h"

/*
 * This file controls arbiters that are assigned to a squad. Other special team usages of arbiters would be in a Play.
 *
 * Arbiters assigned to a squad stay with the vanguard cluster to cloak it and use stasis field when it is advantageous to do so.
 *
 * Arbiters attempt to keep their distance from enemy anti-air units and science vessels, and flee from EMP.
 */

namespace
{
    const double goalWeight = 64.0;
    const int threatWeight = 160;
    const int separationDetectionLimit = 160;
    const double separationWeight = 160.0;

    // The range around an arbiter it will consider using stasis
    const int stasisRange = 12;

    // We try to stay behind the unit closest to the cluster center
    BWAPI::Position desiredPosition(const std::shared_ptr<UnitCluster> &vanguardCluster)
    {
        if (!vanguardCluster) return Map::getMyMain()->getPosition();

        MyUnit centerUnit = nullptr;
        int closestDist = INT_MAX;
        for (auto &unit : vanguardCluster->units)
        {
            int dist = unit->getDistance(vanguardCluster->center);
            if (dist < closestDist)
            {
                closestDist = dist;
                centerUnit = unit;
            }
        }

        if (!centerUnit) return Map::getMyMain()->getPosition();
        return centerUnit->lastPosition;
    }

    MyUnit getStasisArbiter(std::set<MyUnit> &arbiters, int &totalEnergy)
    {
        if (!Players::hasResearched(BWAPI::Broodwar->self(), BWAPI::TechTypes::Stasis_Field)) return nullptr;

        MyUnit best = nullptr;
        int bestEnergy = 99; // Require enough for a stasis
        for (auto &arbiter : arbiters)
        {
            totalEnergy += arbiter->energy;

            // Cast at most one stasis per two seconds
            if ((BWAPI::Broodwar->getFrameCount() - arbiter->lastCastFrame) < 48) return nullptr;

            if (arbiter->energy > bestEnergy)
            {
                bestEnergy = arbiter->energy;
                best = arbiter;
            }
        }

        return best;
    }

    BWAPI::Position getStasisPosition(MyUnit &arbiter, BWAPI::Position centerPosition, int totalEnergy)
    {
        if (!arbiter) return BWAPI::Positions::Invalid;

        auto centerTile = BWAPI::TilePosition(centerPosition);

        auto grid = Players::grid(BWAPI::Broodwar->enemy());

        // Determine how many tanks we want to hit, based on the total energy of our arbiters
        // The more energy we have, the more tanks we want to hit with one cast
        int minimumTargets = 4;
        if (totalEnergy > 200)
        {
            minimumTargets = 3;
        }
        else if (totalEnergy > 300)
        {
            minimumTargets = 2;
        }

        BWAPI::Position best = BWAPI::Positions::Invalid;
        int bestValue = minimumTargets - 1;
        int bestDist = INT_MAX;
        for (int tileY = arbiter->tilePositionY - stasisRange; tileY <= arbiter->tilePositionY + stasisRange; tileY++)
        {
            if (tileY < 0 || tileY >= BWAPI::Broodwar->mapHeight()) continue;

            for (int tileX = arbiter->tilePositionX - stasisRange; tileX <= arbiter->tilePositionX + stasisRange; tileX++)
            {
                if (tileX < 0 || tileX >= BWAPI::Broodwar->mapWidth()) continue;

                // We only stasis open locations to avoid blocking our own units
                if (Map::unwalkableProximity(tileX, tileY) < 4) continue;

                // We only stasis when the vanguard cluster center is reasonably close to the target
                if (Geo::ApproximateDistance(tileX, centerTile.x, tileY, centerTile.y) > 20) continue;

                // This tile is OK, check if any of its walk positions have a good tile
                for (int walkY = tileY << 2; walkY < (tileY + 1) << 2; walkY++)
                {
                    for (int walkX = tileX << 2; walkX < (tileX + 1) << 2; walkX++)
                    {
                        auto value = grid.stasisRange(walkX, walkY);
                        if (value < bestValue) continue;

                        BWAPI::Position pos((walkX << 3) + 4, (walkY << 3) + 4);

                        int dist = arbiter->getDistance(pos);
                        if (value > bestValue || dist < bestDist)
                        {
                            bestValue = value;
                            bestDist = dist;
                            best = pos;
                        }
                    }
                }
            }
        }

        return best;
    }
}

void Squad::executeArbiters()
{
    auto desiredPos = desiredPosition(currentVanguardCluster);

    // Determine if one of our arbiters should cast stasis
    int totalEnergy = 0;
    MyUnit stasisArbiter = getStasisArbiter(arbiters, totalEnergy);
    auto stasisPosition = getStasisPosition(stasisArbiter, desiredPos, totalEnergy);

    // Boids:
    // - Goal (move towards desired position)
    // - Threat (move away from anything that threatens us)
    // - Separation (keep some distance between arbiters so they cloak as large an area as possible)
    // Additionally, we flee from no-go areas (mainly for EMP) and ensure we are always moving
    for (auto &arbiter : arbiters)
    {
        // Special case, if we are in a no-go area, move out of it
        if (NoGoAreas::isNoGo(arbiter->tilePositionX, arbiter->tilePositionY))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(arbiter->id) << "Moving to avoid no-go area";
#endif
            arbiter->moveTo(Boids::AvoidNoGoArea(arbiter.get()));
            continue;
        }

        if (arbiter == stasisArbiter && stasisPosition != BWAPI::Positions::Invalid)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(arbiter->id) << "Order: Stasis at " << BWAPI::WalkPosition(stasisPosition);
#endif

            arbiter->bwapiUnit->useTech(BWAPI::TechTypes::Stasis_Field, stasisPosition);
            continue;
        }

        // Goal
        int goalX = desiredPos.x - arbiter->lastPosition.x;
        int goalY = desiredPos.y - arbiter->lastPosition.y;
        if (goalX != 0 || goalY != 0)
        {
            double goalScale = goalWeight / (double) Geo::ApproximateDistance(goalX, 0, goalY, 0);
            goalX = (int) ((double) goalX * goalScale);
            goalY = (int) ((double) goalY * goalScale);
        }

        // Threat
        int threatX = 0;
        int threatY = 0;
        auto addThreatForType = [&](BWAPI::UnitType type, int radius)
        {
            int detectionLimit = radius + 32;

            for (const auto &unit : Units::allEnemyOfType(type))
            {
                if (!unit->exists()) continue;
                if (!unit->lastPositionValid) continue;
                if (!unit->canAttack(arbiter)) continue;

                auto dist = arbiter->getDistance(unit);
                if (dist > detectionLimit) continue;

                auto vector = Geo::ScaleVector(arbiter->lastPosition - unit->lastPosition, threatWeight);
                if (vector == BWAPI::Positions::Invalid) continue;

                threatX += vector.x;
                threatY += vector.y;
            }
        };
        addThreatForType(BWAPI::UnitTypes::Terran_Science_Vessel, 8 * 8);
        addThreatForType(BWAPI::UnitTypes::Terran_Goliath,
                         Players::weaponRange(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Terran_Goliath.airWeapon()));
        addThreatForType(BWAPI::UnitTypes::Terran_Missile_Turret,
                         Players::weaponRange(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Terran_Missile_Turret.airWeapon()));
        addThreatForType(BWAPI::UnitTypes::Terran_Marine,
                         Players::weaponRange(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Terran_Marine.airWeapon()));
        addThreatForType(BWAPI::UnitTypes::Terran_Ghost,
                         Players::weaponRange(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Terran_Ghost.airWeapon()));
        addThreatForType(BWAPI::UnitTypes::Terran_Bunker,
                         Players::weaponRange(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Terran_Marine.airWeapon()) + 32);
        addThreatForType(BWAPI::UnitTypes::Terran_Wraith,
                         Players::weaponRange(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Terran_Wraith.airWeapon()));
        addThreatForType(BWAPI::UnitTypes::Terran_Valkyrie,
                         Players::weaponRange(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Terran_Valkyrie.airWeapon()));
        addThreatForType(BWAPI::UnitTypes::Terran_Battlecruiser,
                         Players::weaponRange(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Terran_Battlecruiser.airWeapon()));

        // Separation
        int separationX = 0;
        int separationY = 0;
        for (const auto &other : arbiters)
        {
            if (other == arbiter) continue;

            Boids::AddSeparation(arbiter.get(), other, separationDetectionLimit, separationWeight, separationX, separationY);
        }

        auto pos = Boids::ComputePosition(arbiter.get(), {goalX, threatX, separationX}, {goalY, threatY, separationY}, 64, 64);
        if (pos == BWAPI::Positions::Invalid)
        {
            pos = desiredPos;
        }

#if DEBUG_UNIT_BOIDS
        CherryVis::log(arbiter->id) << "Movement boids towards " << BWAPI::WalkPosition(desiredPos)
                                    << ": goal=" << BWAPI::WalkPosition(arbiter->lastPosition + BWAPI::Position(goalX, goalY))
                                    << "; threat=" << BWAPI::WalkPosition(arbiter->lastPosition + BWAPI::Position(threatX, threatY))
                                    << "; separation=" << BWAPI::WalkPosition(arbiter->lastPosition + BWAPI::Position(separationX, separationY))
                                    << "; total=" << BWAPI::WalkPosition(
                arbiter->lastPosition + BWAPI::Position(goalX + threatX + separationX, goalY + threatY + separationY))
                                    << "; target=" << BWAPI::WalkPosition(pos);
#elif DEBUG_UNIT_ORDERS
        CherryVis::log(unit->id) << "Move boids: Moving to " << BWAPI::WalkPosition(pos);
#endif

        // If the arbiter is close to its desired position and not under threat, allow it to attack
        if (threatX == 0 && threatY == 0 && arbiter->getDistance(desiredPos) < 64)
        {
            arbiter->attackMove(pos);
        }
        else
        {
            arbiter->moveTo(pos, true);
        }

        // When we have multiple arbiters, the others stay with the cluster vanguard
        if (currentVanguardCluster && currentVanguardCluster->vanguard)
        {
            desiredPos = currentVanguardCluster->vanguard->lastPosition;
        }
    }
}