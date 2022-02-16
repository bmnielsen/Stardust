#include "ShuttleHarass.h"

#include "Map.h"
#include "Players.h"
#include "Units.h"
#include "Strategist.h"
#include "Geo.h"

namespace
{
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

        // Move away from target if there is an air threat
        // TODO: Path around
        if (grid.airThreat(ahead) > 0)
        {
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
            return;
        }

        shuttle->moveTo(ahead);
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
            for (auto &unit : Units::allEnemyOfType(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode))
            {
                if (!unit->lastPositionValid) continue;

                int dist = PathFinding::GetGroundDistance(cargoAndTarget.first->lastPosition,
                                                          unit->lastPosition,
                                                          cargoAndTarget.first->type,
                                                          PathFinding::PathFindingOptions::UseNeighbouringBWEMArea);
                if (dist != -1 && dist < bestDist)
                {
                    bestDist = dist;
                    target = unit;
                }
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
        Log::Get() << "ZEALOT BOMB";
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
            if (it != cargoAndTargets.end() && it->second && it->second->exists() && it->second->lastPositionValid &&
                grid.airThreat(it->second->lastPosition) == 0)
            {
                target = it->second;
            }

            if (!target)
            {
                int bestDist = INT_MAX;
                for (auto &unit : Units::allEnemyOfType(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode))
                {
                    if (!unit->lastPositionValid) continue;
                    if (grid.airThreat(unit->lastPosition) > 0) continue;

                    int dist = shuttle->getDistance(unit);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        target = unit;
                    }
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
                }
                else
                {
                    moveAvoidingThreats(grid, shuttle, Map::getMyMain()->getPosition());
                }
                continue;
            }

            // Drop on the target
            auto distToTarget = shuttle->getDistance(target);
            if (distToTarget > 16)
            {
                moveAvoidingThreats(grid, shuttle, target->lastPosition);
            }
            else
            {
                shuttle->unload(cargo->bwapiUnit);
            }
            continue;
        }

        // We have just unloaded cargo
        if (cargo && cargoAndTargets.find(cargo) != cargoAndTargets.end())
        {
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
            }
            else
            {
                shuttle->load(cargo->bwapiUnit);
            }
            continue;
        }

        // We don't have cargo

        // If we don't have a main army vanguard, move the shuttle to our main base until we do
        if (!mainArmyVanguard)
        {
            moveAvoidingThreats(grid, shuttle, Map::getMyMain()->getPosition());
            continue;
        }

        // Move to the vanguard center
        moveAvoidingThreats(grid, shuttle, mainArmyVanguard->center);

        // If the shuttle is near the vanguard center, request a zealot
        auto vanguardDist = shuttle->getDistance(mainArmyVanguard->center);
        if (vanguardDist < 500 + mainArmyVanguard->lineRadius)
        {
            status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Zealot, shuttle->lastPosition, 640);
        }
    }
}
