#include "ShuttleHarass.h"

#include "Map.h"
#include "Players.h"
#include "Units.h"
#include "Strategist.h"
#include "Geo.h"

#include "DebugFlag_UnitOrders.h"

namespace
{
    // Define some positions for use in searching outwards from a point at tile resolution
    const BWAPI::Position surroundingPositions[] = {
            BWAPI::Position(0, -48),
            BWAPI::Position(16, -48),
            BWAPI::Position(32, -32),
            BWAPI::Position(48, -16),
            BWAPI::Position(48, 0),
            BWAPI::Position(48, 16),
            BWAPI::Position(32, 32),
            BWAPI::Position(16, 48),
            BWAPI::Position(0, 48),
            BWAPI::Position(-16, 48),
            BWAPI::Position(-32, 32),
            BWAPI::Position(-48, 16),
            BWAPI::Position(-48, 0),
            BWAPI::Position(-48, -16),
            BWAPI::Position(-32, -32),
            BWAPI::Position(-16, -48)};

    BWAPI::Position scaledPosition(BWAPI::Position currentPosition, BWAPI::Position vector, int length)
    {
        auto scaledVector = Geo::ScaleVector(vector, length);
        if (scaledVector == BWAPI::Positions::Invalid) return BWAPI::Positions::Invalid;

        return currentPosition + scaledVector;
    }

    void moveAvoidingThreats(const Grid &grid, const MyUnit &shuttle, BWAPI::Position target)
    {
        // Check for threats one-and-a-half tiles ahead
        auto ahead = scaledPosition(shuttle->lastPosition, target - shuttle->lastPosition, 48);
        if (!ahead.isValid())
        {
            shuttle->moveTo(target);
            return;
        }
        if (grid.airThreat(ahead) == 0)
        {
            shuttle->moveTo(ahead);
            return;
        }

        // Get the surrounding position closest to the target that is not under air threat
        int bestDist = INT_MAX;
        BWAPI::Position best = BWAPI::Positions::Invalid;
        for (auto &offset : surroundingPositions)
        {
            auto here = shuttle->lastPosition + offset;
            if (!here.isValid()) continue;
            if (grid.airThreat(here) > 0) continue;

            int dist = here.getApproxDistance(target);
            if (dist < bestDist)
            {
                best = here;
                bestDist = dist;
            }
        }

        // Move towards the best tile if possible
        if (best != BWAPI::Positions::Invalid)
        {
            shuttle->moveTo(best);
            return;
        }

        // We couldn't find a better tile to move to, so just move away from the target position
        auto behind = scaledPosition(shuttle->lastPosition, shuttle->lastPosition - target, 64);
        if (behind.isValid())
        {
            shuttle->moveTo(behind);
        }
        else
        {
            // Default to main base location when we don't have anywhere better to go
            shuttle->moveTo(Map::getMyMain()->getPosition());
        }
    }
}

void ShuttleHarass::update()
{
    // Always request shuttles so all unassigned shuttles get assigned to this play
    // This play is always at lowest priority with respect to other plays utilizing shuttles
    status.unitRequirements.emplace_back(10, BWAPI::UnitTypes::Protoss_Shuttle, Map::getMyMain()->getPosition());

    // Micro dropped units
    for (auto &cargoAndTarget : cargoAndTargets)
    {
        // We are still loaded - wait until we get dropped
        if (cargoAndTarget.first->bwapiUnit->isLoaded())
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(cargoAndTarget.first->id) << "Waiting to be unloaded";
#endif
            continue;
        }

        // Target is dead
        if (cargoAndTarget.second == nullptr || !cargoAndTarget.second->exists())
        {
            Unit target = nullptr;
            int bestDist = INT_MAX;
            auto processUnit = [&](const Unit &unit)
            {
                if (!unit->lastPositionValid) return;
                if (unit->bwapiUnit->isStasised()) return;

                int dist = PathFinding::GetGroundDistance(cargoAndTarget.first->lastPosition,
                                                          unit->lastPosition,
                                                          cargoAndTarget.first->type,
                                                          PathFinding::PathFindingOptions::UseNeighbouringBWEMArea);
                if (dist != -1 && dist < bestDist)
                {
                    bestDist = dist;
                    target = unit;
                }
            };

            for (auto &unit : Units::allEnemyOfType(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode))
            {
                processUnit(unit);
            }
            for (auto &unit : Units::allEnemyOfType(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode))
            {
                processUnit(unit);
            }

            if (!target)
            {
#if DEBUG_UNIT_ORDERS
                CherryVis::log(cargoAndTarget.first->id) << "No targets available";
#endif
                status.removedUnits.emplace_back(cargoAndTarget.first);
                continue;
            }

#if DEBUG_UNIT_ORDERS
            CherryVis::log(cargoAndTarget.first->id) << "Retargeted to " << (*target);
#endif
            cargoAndTarget.second = target;
        }

        // Attack the target
        std::vector<std::pair<MyUnit, Unit>> emptyUnitsAndTargets;
        cargoAndTarget.first->attackUnit(cargoAndTarget.second, emptyUnitsAndTargets);
    }

    // Micro shuttles
    auto &grid = Players::grid(BWAPI::Broodwar->enemy());
    auto mainArmyPlay = Strategist::getMainArmyPlay();
    auto mainArmySquad = mainArmyPlay ? mainArmyPlay->getSquad() : nullptr;
    auto mainArmyVanguard = mainArmySquad ? mainArmySquad->vanguardCluster() : nullptr;
    for (auto &shuttleAndCargo : shuttlesAndCargo)
    {
        auto &shuttle = shuttleAndCargo.first;
        auto &cargo = shuttleAndCargo.second;

        // We have loaded cargo
        if (cargo && cargo->bwapiUnit->isLoaded())
        {
            // Ensure we have a target
            Unit target = nullptr;
            auto it = cargoAndTargets.find(cargo);
            if (it != cargoAndTargets.end() && it->second)
            {
                target = it->second;

                if (!target->exists())
                {
#if DEBUG_UNIT_ORDERS
                    CherryVis::log(shuttle->id) << "Clearing target as it no longer exists";
#endif
                    target = nullptr;
                }
                else if (!target->lastPositionValid)
                {
#if DEBUG_UNIT_ORDERS
                    CherryVis::log(shuttle->id) << "Clearing target as its last position is no longer valid";
#endif
                    target = nullptr;
                }
                else if (shuttle->getDistance(target) > 48 && grid.airThreat(target->lastPosition) > 0)
                {
#if DEBUG_UNIT_ORDERS
                    CherryVis::log(shuttle->id) << "Clearing target as its last position is now under air threat";
#endif
                    target = nullptr;
                }
            }

            if (!target)
            {
                int bestDist = INT_MAX;
                auto processUnit = [&](const Unit &unit)
                {
                    if (!unit->lastPositionValid) return;
                    if (unit->bwapiUnit->isStasised()) return;

                    int dist = shuttle->getDistance(unit);

                    if (dist > 48 && grid.airThreat(unit->lastPosition) > 0) return;

                    if (unit->type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) dist = (dist * 3) / 2;

                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        target = unit;
                    }
                };
                for (auto &unit : Units::allEnemyOfType(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode))
                {
                    processUnit(unit);
                }
                for (auto &unit : Units::allEnemyOfType(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode))
                {
                    processUnit(unit);
                }

#if DEBUG_UNIT_ORDERS
                if (target)
                {
                    CherryVis::log(shuttle->id) << "Selected target " << (*target);
                }
#endif
                cargoAndTargets[cargo] = target;
            }

            // If we don't have a target, hang around the vanguard cluster until we get one
            if (!target)
            {
                if (mainArmyVanguard)
                {
                    moveAvoidingThreats(grid, shuttle, mainArmyVanguard->center);
#if DEBUG_UNIT_ORDERS
                    CherryVis::log(shuttle->id) << "Cargo loaded; no target; moving to vanguard @ " << BWAPI::WalkPosition(mainArmyVanguard->center);
#endif
                }
                else
                {
                    moveAvoidingThreats(grid, shuttle, Map::getMyMain()->getPosition());
#if DEBUG_UNIT_ORDERS
                    CherryVis::log(shuttle->id) << "Cargo loaded; no target; moving to main";
#endif
                }
                continue;
            }

            // Drop on the target
            auto distToTarget = shuttle->getDistance(target);
            if (distToTarget > 16)
            {
                moveAvoidingThreats(grid, shuttle, target->lastPosition);
#if DEBUG_UNIT_ORDERS
                CherryVis::log(shuttle->id) << "Cargo loaded; moving to target " << BWAPI::WalkPosition(target->lastPosition);
#endif
            }
            else
            {
                shuttle->unload(cargo->bwapiUnit);
#if DEBUG_UNIT_ORDERS
                CherryVis::log(shuttle->id) << "Cargo loaded; unloading on target " << BWAPI::WalkPosition(target->lastPosition);
#endif
            }
            continue;
        }

        // We have just unloaded cargo
        if (cargo && cargoAndTargets.find(cargo) != cargoAndTargets.end())
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(shuttle->id) << "Cleared cargo";
#endif

            // Clear our cargo and fall through
            cargo = nullptr;
        }

        // We have cargo awaiting pickup
        if (cargo)
        {
            cargo->moveTo(shuttle->lastPosition);

            auto distToCargo = shuttle->getDistance(cargo);
            if (distToCargo > 100)
            {
                moveAvoidingThreats(grid, shuttle, cargo->lastPosition);
#if DEBUG_UNIT_ORDERS
                CherryVis::log(shuttle->id) << "Moving to cargo " << (*cargo);
#endif
            }
            else
            {
                shuttle->load(cargo->bwapiUnit);
#if DEBUG_UNIT_ORDERS
                CherryVis::log(shuttle->id) << "Loading cargo " << (*cargo);
#endif
            }
            continue;
        }

        // We don't have cargo

        // If we don't have a main army vanguard, move the shuttle to our main base until we do
        if (!mainArmyVanguard)
        {
            moveAvoidingThreats(grid, shuttle, Map::getMyMain()->getPosition());
#if DEBUG_UNIT_ORDERS
            CherryVis::log(shuttle->id) << "Waiting for vanguard";
#endif
            continue;
        }

        // Move to the vanguard center
        moveAvoidingThreats(grid, shuttle, mainArmyVanguard->center);

        // If the shuttle is near the vanguard center, request a zealot
        auto vanguardDist = shuttle->getDistance(mainArmyVanguard->center);
        if (vanguardDist < (500 + mainArmyVanguard->lineRadius))
        {
            status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Zealot, shuttle->lastPosition, 640);
#if DEBUG_UNIT_ORDERS
            CherryVis::log(shuttle->id) << "Requesting zealot";
#endif
        }
    }
}
