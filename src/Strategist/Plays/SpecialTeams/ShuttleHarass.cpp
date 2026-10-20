#include "ShuttleHarass.h"

#include "Map.h"
#include "Players.h"
#include "Units.h"
#include "Strategist.h"
#include "Geo.h"
#include "UnitUtil.h"

#include <cfloat>

#include "DebugFlag_UnitOrders.h"

namespace
{
    const int HALT_DISTANCE = UnitHelper::HaltDistance(BWAPI::UnitTypes::Protoss_Shuttle) + 16;

    /* Define some positions for use in searching outwards from a point at tile resolution */
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
        auto scaledVector = Geom::ScaleVector(vector, length);
        if (scaledVector == BWAPI::Positions::Invalid) return BWAPI::Positions::Invalid;

        return currentPosition + scaledVector;
    }

    void movePreservingSpeed(const OwnedUnit &shuttle, BWAPI::Position target)
    {
        int span = shuttle->prevPos.getApproxDistance(target);
        if (span == 0)
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(shuttle->ident) << "Move to " << BWAPI::WalkPosition(target) << ": on top; moving towards main";
#endif

            /* We're directly on top of the target, so we just need to move away */
            shuttle->moveTo(Map::getMyMain()->getPosition());
            return;
        }

        /* If the target is far enough away, just move directly */
        if (span >= HALT_DISTANCE)
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(shuttle->ident) << "Move to " << BWAPI::WalkPosition(target) << ": far away; moving directly";
#endif
            shuttle->moveTo(target);
            return;
        }

        /* Otherwise get a position that overshoots the target sufficiently */
        auto scaledVector = Geom::ScaleVector(target - shuttle->prevPos, HALT_DISTANCE);
        auto scaledTarget = shuttle->prevPos + scaledVector;
        if (!scaledTarget.isValid())
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(shuttle->ident) << "Move to " << BWAPI::WalkPosition(target) << ": near map edge; moving directly";
#endif

            shuttle->moveTo(target);
            return;
        }

#if DEBUG_UNIT_ORDERS
        Telemetry::log(shuttle->ident) << "Move to " << BWAPI::WalkPosition(target) << ": moving to " << BWAPI::WalkPosition(scaledTarget);
#endif

        shuttle->moveTo(scaledTarget);
    }

    void moveAvoidingThreats(const Grid &grid, const OwnedUnit &shuttle, BWAPI::Position target)
    {
        /* Check for threats one-and-a-half tiles ahead */
        auto ahead = scaledPosition(shuttle->prevPos, target - shuttle->prevPos, 48);
        if (!ahead.isValid() || grid.airThreat(ahead) == 0)
        {
            movePreservingSpeed(shuttle, target);
            return;
        }

        /* Get the surrounding position closest to the target that is not under air threat */
        int bestDist = INT_MAX;
        BWAPI::Position best = BWAPI::Positions::Invalid;
        for (auto &offset : surroundingPositions)
        {
            auto here = shuttle->prevPos + offset;
            if (!here.isValid()) continue;
            if (grid.airThreat(here) > 0) continue;

            int span = here.getApproxDistance(target);
            if (span < bestDist)
            {
                best = here;
                bestDist = span;
            }
        }

        /* Move towards the best tile if possible */
        if (best != BWAPI::Positions::Invalid)
        {
            movePreservingSpeed(shuttle, best);
            return;
        }

        /* We couldn't find a better tile to move to, so just move away from the target position */
        auto behind = scaledPosition(shuttle->prevPos, shuttle->prevPos - target, 64);
        if (behind.isValid())
        {
            movePreservingSpeed(shuttle, behind);
        }
        else
        {
            /* Default to main base location when we don't have anywhere better to go */
            shuttle->moveTo(Map::getMyMain()->getPosition());
        }
    }
}

void ShuttleHarass::update()
{
    /* Always request shuttles so all unassigned shuttles get assigned to this play */
    /* This play is always at lowest priority with respect to other plays utilizing shuttles */
    status.unitRequirements.emplace_back(10, BWAPI::UnitTypes::Protoss_Shuttle, Map::getMyMain()->getPosition());

    /* Micro dropped units */
    for (auto &cargoAndTarget : cargoAndTargets)
    {
        /* We are still loaded - wait until we get dropped */
        if (cargoAndTarget.first->rawUnit->isLoaded())
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(cargoAndTarget.first->ident) << "Waiting to be unloaded";
#endif
            continue;
        }

        /* Target is dead */
        if (cargoAndTarget.second == nullptr || !cargoAndTarget.second->exists())
        {
            Unit target = nullptr;
            int bestDist = INT_MAX;
            auto processUnit = [&](const Unit &gameUnit)
            {
                if (!gameUnit->lastPositionValid) return;
                if (gameUnit->rawUnit->isStasised()) return;

                int span = Pathfinder::GetGroundDistance(cargoAndTarget.first->prevPos,
                                                          gameUnit->prevPos,
                                                          cargoAndTarget.first->type,
                                                          Pathfinder::PathFindingOptions::UseNeighbouringBWEMArea);
                if (span != -1 && span < bestDist)
                {
                    bestDist = span;
                    target = gameUnit;
                }
            };

            for (auto &gameUnit : UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode))
            {
                processUnit(gameUnit);
            }
            for (auto &gameUnit : UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode))
            {
                processUnit(gameUnit);
            }

            if (!target)
            {
#if DEBUG_UNIT_ORDERS
                Telemetry::log(cargoAndTarget.first->ident) << "No targets available";
#endif
                status.removedUnits.emplace_back(cargoAndTarget.first);
                continue;
            }

#if DEBUG_UNIT_ORDERS
            Telemetry::log(cargoAndTarget.first->ident) << "Retargeted to " << (*target);
#endif
            cargoAndTarget.second = target;
        }

        /* Attack the target */
        cargoAndTarget.first->attackUnit(cargoAndTarget.second);
    }

    /* Micro shuttles */
    auto &grid = PlayerInfo::grid(BWAPI::Broodwar->enemy());
    auto mainArmyPlay = Director::getMainArmyPlay();
    auto mainArmySquad = mainArmyPlay ? mainArmyPlay->getSquad() : nullptr;
    auto mainArmyVanguard = mainArmySquad ? mainArmySquad->vanguardCluster() : nullptr;
    for (auto &shuttleAndCargo : shuttlesAndCargo)
    {
        auto &shuttle = shuttleAndCargo.first;
        auto &cargo = shuttleAndCargo.second;

        /* If all cargo is loaded, update their target */
        if (cargo.size() >= 2 &&
            std::all_of(cargo.begin(), cargo.end(), [](const OwnedUnit& gameUnit){ return gameUnit->rawUnit->isLoaded(); }))
        {
            /* Ensure we have a target */
            Unit target = nullptr;
            auto iterRef = cargoAndTargets.find(*cargo.begin());
            if (iterRef != cargoAndTargets.end() && iterRef->second)
            {
                target = iterRef->second;

                if (!target->exists())
                {
#if DEBUG_UNIT_ORDERS
                    Telemetry::log(shuttle->ident) << "Clearing target as it no longer exists";
#endif
                    target = nullptr;
                }
                else if (!target->lastPositionValid)
                {
#if DEBUG_UNIT_ORDERS
                    Telemetry::log(shuttle->ident) << "Clearing target as its last position is no longer valid";
#endif
                    target = nullptr;
                }
                else if (shuttle->getDistance(target) > 48 && grid.airThreat(target->prevPos) > 0)
                {
#if DEBUG_UNIT_ORDERS
                    Telemetry::log(shuttle->ident) << "Clearing target as its last position is now under air threat";
#endif
                    target = nullptr;
                }
                else if (grid.groundThreat(target->prevPos) - grid.staticGroundThreat(target->prevPos) > 30)
                {
#if DEBUG_UNIT_ORDERS
                    Telemetry::log(shuttle->ident) << "Clearing target as its last position is now under non-static ground threat";
#endif
                    target = nullptr;
                }
            }

            if (!target)
            {
                double bestScore = DBL_MAX;
                auto processUnit = [&](const Unit &gameUnit)
                {
                    if (!gameUnit->lastPositionValid) return;
                    if (gameUnit->rawUnit->isStasised()) return;

                    /* Skip tanks that have been repaired in the last 30 seconds */
                    if (gameUnit->lastHealFrame > (frameNow - 640)) return;

                    int span = shuttle->getDistance(gameUnit);

                    /* Don't drop on units covered by anti-air */
                    if (span > 48 && grid.airThreat(gameUnit->prevPos) > 0) return;

                    /* Don't drop on units that are covered by excessive non-static ground threats */
                    if (grid.groundThreat(gameUnit->prevPos) - grid.staticGroundThreat(gameUnit->prevPos) > 30) return;

                    /* Prefer sieged tanks */
                    double score = span;
                    if (gameUnit->type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) score *= 1.5;

                    /* Prefer damaged tanks */
                    double percentDamaged = (double)gameUnit->health / BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.maxHitPoints();
                    score /= std::max(percentDamaged, 0.25);

                    if (score < bestScore)
                    {
                        bestScore = score;
                        target = gameUnit;
                    }
                };
                for (auto &gameUnit : UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode))
                {
                    processUnit(gameUnit);
                }
                for (auto &gameUnit : UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode))
                {
                    processUnit(gameUnit);
                }

#if DEBUG_UNIT_ORDERS
                if (target)
                {
                    Telemetry::log(shuttle->ident) << "Selected target " << (*target);
                }
#endif
                for (auto &gameUnit : cargo)
                {
                    cargoAndTargets[gameUnit] = target;
                }
            }

            /* If we don't have a target, hang around the vanguard cluster until we get one */
            if (!target)
            {
                if (mainArmyVanguard)
                {
                    moveAvoidingThreats(grid, shuttle, mainArmyVanguard->midpoint);
#if DEBUG_UNIT_ORDERS
                    Telemetry::log(shuttle->ident) << "Cargo loaded; no target; moving to vanguard @ " << BWAPI::WalkPosition(mainArmyVanguard->midpoint);
#endif
                }
                else
                {
                    moveAvoidingThreats(grid, shuttle, Map::getMyMain()->getPosition());
#if DEBUG_UNIT_ORDERS
                    Telemetry::log(shuttle->ident) << "Cargo loaded; no target; moving to main";
#endif
                }
                continue;
            }
        }

        /* If we are in the drop phase, drop loaded cargo on the target */
        Unit firstTarget = nullptr;
        auto firstTargetExists = [&]()
        {
            if (cargo.empty()) return false;
            auto firstTargetIt = cargoAndTargets.find(*cargo.begin());
            if (firstTargetIt == cargoAndTargets.end()) return false;
            if (!firstTargetIt->second) return false;
            if (!firstTargetIt->second->exists()) return false;

            firstTarget = firstTargetIt->second;
            return true;
        };
        if (!cargo.empty() && firstTargetExists())
        {
            /* If all of our cargo is dropped, clear it and fall through to get new cargo */
            if (std::all_of(cargo.begin(), cargo.end(), [](const OwnedUnit& gameUnit){ return !gameUnit->rawUnit->isLoaded(); }))
            {
#if DEBUG_UNIT_ORDERS
                Telemetry::log(shuttle->ident) << "Cleared cargo";
#endif
                cargo.clear();
            }
            else
            {
                /* Drop loaded units on the target */
                auto distToTarget = shuttle->getDistance(firstTarget);
                if (distToTarget > 16)
                {
                    moveAvoidingThreats(grid, shuttle, firstTarget->prevPos);
#if DEBUG_UNIT_ORDERS
                    Telemetry::log(shuttle->ident) << "Cargo loaded; moving to target " << BWAPI::WalkPosition(firstTarget->prevPos);
#endif
                }
                else
                {
                    for (auto &gameUnit : cargo)
                    {
                        if (gameUnit->rawUnit->isLoaded())
                        {
                            shuttle->unload(gameUnit->rawUnit);
                            break;
                        }
                    }
#if DEBUG_UNIT_ORDERS
                    Telemetry::log(shuttle->ident) << "Cargo loaded; unloading on target " << BWAPI::WalkPosition(firstTarget->prevPos);
#endif
                }
                continue;
            }
        }

        /* We have cargo awaiting pickup */
        if (!cargo.empty() &&
            std::any_of(cargo.begin(), cargo.end(), [](const OwnedUnit& gameUnit){ return !gameUnit->rawUnit->isLoaded(); }))
        {
            for (auto &gameUnit : cargo)
            {
                if (!gameUnit->rawUnit->isLoaded())
                {
                    gameUnit->moveTo(shuttle->prevPos);

                    auto distToCargo = shuttle->getDistance(gameUnit);
                    if (distToCargo > 100)
                    {
                        moveAvoidingThreats(grid, shuttle, gameUnit->prevPos);
#if DEBUG_UNIT_ORDERS
                        Telemetry::log(shuttle->ident) << "Moving to cargo " << (*gameUnit);
#endif
                    }
                    else
                    {
                        shuttle->load(gameUnit->rawUnit);
#if DEBUG_UNIT_ORDERS
                        Telemetry::log(shuttle->ident) << "Loading cargo " << (*gameUnit);
#endif
                    }
                    break;
                }
            }

            continue;
        }

        /* We need more cargo */

        /* If we don't have a main army vanguard, move the shuttle to our main base until we do */
        if (!mainArmyVanguard)
        {
            moveAvoidingThreats(grid, shuttle, Map::getMyMain()->getPosition());
#if DEBUG_UNIT_ORDERS
            Telemetry::log(shuttle->ident) << "Waiting for vanguard";
#endif
            continue;
        }

        /* Move to the vanguard center */
        moveAvoidingThreats(grid, shuttle, mainArmyVanguard->midpoint);

        /* If the shuttle is near the vanguard center, and there is a potential target, request a zealot */
        auto vanguardDist = shuttle->getDistance(mainArmyVanguard->midpoint);
        if (vanguardDist < (500 + mainArmyVanguard->lineRadius) && UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode))
        {
            status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Zealot, shuttle->prevPos, 640);
#if DEBUG_UNIT_ORDERS
            Telemetry::log(shuttle->ident) << "Requesting zealot";
#endif
        }
    }
}
