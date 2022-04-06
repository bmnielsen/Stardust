#include "ShuttleHarass.h"

#include "Map.h"
#include "Players.h"
#include "Units.h"
#include "Strategist.h"
#include "Geo.h"

#include <cfloat>

#include "DebugFlag_UnitOrders.h"

#define HALT_DISTANCE 96

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

    void movePreservingSpeed(const MyUnit &shuttle, BWAPI::Position target)
    {
        int dist = shuttle->lastPosition.getApproxDistance(target);
        if (dist == 0)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(shuttle->id) << "Move to " << BWAPI::WalkPosition(target) << ": on top; moving towards main";
#endif

            // We're directly on top of the target, so we just need to move away
            shuttle->moveTo(Map::getMyMain()->getPosition());
            return;
        }

        // If the target is far enough away, just move directly
        if (dist > HALT_DISTANCE)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(shuttle->id) << "Move to " << BWAPI::WalkPosition(target) << ": far away; moving directly";
#endif
            shuttle->moveTo(target);
            return;
        }

        // Otherwise get a position that overshoots the target sufficiently
        auto scaledVector = Geo::ScaleVector(target - shuttle->lastPosition, HALT_DISTANCE);
        auto scaledTarget = shuttle->lastPosition + scaledVector;
        if (!scaledTarget.isValid())
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(shuttle->id) << "Move to " << BWAPI::WalkPosition(target) << ": near map edge; moving directly";
#endif

            shuttle->moveTo(target);
            return;
        }

#if DEBUG_UNIT_ORDERS
        CherryVis::log(shuttle->id) << "Move to " << BWAPI::WalkPosition(target) << ": moving to " << BWAPI::WalkPosition(scaledTarget);
#endif

        shuttle->moveTo(scaledTarget);
    }

    void moveAvoidingThreats(const Grid &grid, const MyUnit &shuttle, BWAPI::Position target)
    {
        // Check for threats one-and-a-half tiles ahead
        auto ahead = scaledPosition(shuttle->lastPosition, target - shuttle->lastPosition, 48);
        if (!ahead.isValid() || grid.airThreat(ahead) == 0)
        {
            movePreservingSpeed(shuttle, target);
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
            movePreservingSpeed(shuttle, best);
            return;
        }

        // We couldn't find a better tile to move to, so just move away from the target position
        auto behind = scaledPosition(shuttle->lastPosition, shuttle->lastPosition - target, 64);
        if (behind.isValid())
        {
            movePreservingSpeed(shuttle, behind);
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

        // If all cargo is loaded, update their target
        if (cargo.size() >= 2 &&
            std::all_of(cargo.begin(), cargo.end(), [](const MyUnit& unit){ return unit->bwapiUnit->isLoaded(); }))
        {
            // Ensure we have a target
            Unit target = nullptr;
            auto it = cargoAndTargets.find(*cargo.begin());
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
                double bestScore = DBL_MAX;
                auto processUnit = [&](const Unit &unit)
                {
                    if (!unit->lastPositionValid) return;
                    if (unit->bwapiUnit->isStasised()) return;

                    // Skip tanks that have been repaired in the last 30 seconds
                    if (unit->lastHealFrame > (currentFrame - 640)) return;

                    int dist = shuttle->getDistance(unit);

                    // Don't drop on units covered by anti-air
                    if (dist > 48 && grid.airThreat(unit->lastPosition) > 0) return;

                    // Prefer sieged tanks
                    double score = dist;
                    if (unit->type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) score *= 1.5;

                    // Prefer damaged tanks
                    double percentDamaged = (double)unit->health / BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.maxHitPoints();
                    score /= std::max(percentDamaged, 0.25);

                    if (score < bestScore)
                    {
                        bestScore = score;
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
                for (auto &unit : cargo)
                {
                    cargoAndTargets[unit] = target;
                }
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
        }

        // If we are in the drop phase, drop loaded cargo on the target
        if (!cargo.empty() && cargoAndTargets.find(*cargo.begin()) != cargoAndTargets.end())
        {
            // If all of our cargo is dropped, clear it and fall through to get new cargo
            if (std::all_of(cargo.begin(), cargo.end(), [](const MyUnit& unit){ return !unit->bwapiUnit->isLoaded(); }))
            {
#if DEBUG_UNIT_ORDERS
                CherryVis::log(shuttle->id) << "Cleared cargo";
#endif
                cargo.clear();
            }
            else
            {
                auto target = cargoAndTargets[*cargo.begin()];

                // Drop loaded units on the target
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
                    for (auto &unit : cargo)
                    {
                        if (unit->bwapiUnit->isLoaded())
                        {
                            shuttle->unload(unit->bwapiUnit);
                            break;
                        }
                    }
#if DEBUG_UNIT_ORDERS
                    CherryVis::log(shuttle->id) << "Cargo loaded; unloading on target " << BWAPI::WalkPosition(target->lastPosition);
#endif
                }
                continue;
            }
        }

        // We have cargo awaiting pickup
        if (!cargo.empty() &&
            std::any_of(cargo.begin(), cargo.end(), [](const MyUnit& unit){ return !unit->bwapiUnit->isLoaded(); }))
        {
            for (auto &unit : cargo)
            {
                if (!unit->bwapiUnit->isLoaded())
                {
                    unit->moveTo(shuttle->lastPosition);

                    auto distToCargo = shuttle->getDistance(unit);
                    if (distToCargo > 100)
                    {
                        moveAvoidingThreats(grid, shuttle, unit->lastPosition);
#if DEBUG_UNIT_ORDERS
                        CherryVis::log(shuttle->id) << "Moving to cargo " << (*unit);
#endif
                    }
                    else
                    {
                        shuttle->load(unit->bwapiUnit);
#if DEBUG_UNIT_ORDERS
                        CherryVis::log(shuttle->id) << "Loading cargo " << (*unit);
#endif
                    }
                    break;
                }
            }

            continue;
        }

        // We need more cargo

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

        // If the shuttle is near the vanguard center, and there is a potential target, request a zealot
        auto vanguardDist = shuttle->getDistance(mainArmyVanguard->center);
        if (vanguardDist < (500 + mainArmyVanguard->lineRadius) && Units::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode))
        {
            status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Zealot, shuttle->lastPosition, 640);
#if DEBUG_UNIT_ORDERS
            CherryVis::log(shuttle->id) << "Requesting zealot";
#endif
        }
    }
}
