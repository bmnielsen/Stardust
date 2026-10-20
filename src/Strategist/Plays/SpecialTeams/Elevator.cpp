#include "Elevator.h"

#include "Map.h"
#include "General.h"
#include "Workers.h"
#include "Units.h"
#include "PathFinding.h"
#include "Strategist.h"
#include "Geo.h"
#include "Opponent.h"
#include "UnitUtil.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define CVIS_ELEVATORTILES_HEATMAP false
#endif

#if INSTRUMENTATION_ENABLED
#define CVIS_LOG_ELEVATOR_STATE true
#endif

namespace
{
    const int SHUTTLE_HALT_DISTANCE = UnitHelper::HaltDistance(BWAPI::UnitTypes::Protoss_Shuttle) + 16;

    bool validElevatorPosition(BWAPI::TilePosition tilePos)
    {
        auto validAndWalkable = [](BWAPI::TilePosition tilePos)
        {
            return tilePos.isValid() && Map::isWalkable(tilePos);
        };

        /* The tile itself must be valid and walkable */
        if (!validAndWalkable(tilePos)) return false;

        /* At least three of its direct neighbours must be valid and walkable */
        int count = 0;
        if (validAndWalkable(tilePos + BWAPI::TilePosition(1, 0))) count++;
        if (validAndWalkable(tilePos + BWAPI::TilePosition(-1, 0))) count++;
        if (validAndWalkable(tilePos + BWAPI::TilePosition(0, 1))) count++;
        if (validAndWalkable(tilePos + BWAPI::TilePosition(0, -1))) count++;
        return count >= 3;
    }

    BWAPI::Position scaledPosition(BWAPI::Position currentPosition, BWAPI::Position vector, int length)
    {
        auto scaledVector = Geom::ScaleVector(vector, length);
        if (scaledVector == BWAPI::Positions::Invalid) return BWAPI::Positions::Invalid;

        return currentPosition + scaledVector;
    }

    void shuttleMove(OwnedUnit &shuttle, BWAPI::Position pt)
    {
        /* Always move towards a position at least halt distance away */
        auto moveTarget = scaledPosition(shuttle->prevPos, pt - shuttle->prevPos, SHUTTLE_HALT_DISTANCE);

        /* If it is invalid, it means we are exactly on our target position, so just use our main until we get somewhere else */
        if (moveTarget == BWAPI::Positions::Invalid) moveTarget = Map::getMyMain()->getPosition();

        /* If it is off the map, move directly to the target */
        if (!moveTarget.isValid()) moveTarget = pt;

        shuttle->moveTo(moveTarget);
    }

    void drop(OwnedUnit &shuttle, BWAPI::Position pt)
    {
        if (shuttle->rawUnit->getLoadedUnits().empty()) return;

        if (shuttle->rawUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Unload)
        {
            /* Wait 12 frames after ordering an unload */
            if (shuttle->lastCommandFrame < (frameNow - 12)) return;

            /* Move to the drop position on the 12th frame */
            if (shuttle->lastCommandFrame == (frameNow - 12))
            {
                shuttleMove(shuttle, pt);
                return;
            }
        }

        /* If we are close enough to the target position, start to drop */
        /* Otherwise move towards the position */
        if (shuttle->getDistance(pt) < 32)
        {
            shuttle->unload(*(shuttle->rawUnit->getLoadedUnits().begin()));
        }
        else
        {
            shuttleMove(shuttle, pt);
        }
    }

    void pickup(OwnedUnit &shuttle, OwnedUnit &cargo)
    {
        if (shuttle->rawUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Load)
        {
            /* Wait 12 frames after ordering a load */
            if (shuttle->lastCommandFrame < (frameNow - 12)) return;

            /* Move to the load position on the 12th frame */
            if (shuttle->lastCommandFrame == (frameNow - 12))
            {
                shuttleMove(shuttle, cargo->prevPos);
                return;
            }
        }

        /* If we are close enough to the cargo, start to load */
        /* Otherwise move towards its position */
        if (shuttle->getDistance(cargo) < 12)
        {
            shuttle->load(cargo->rawUnit);
        }
        else
        {
            shuttleMove(shuttle, cargo->prevPos);
        }
    }
}

Elevator::Elevator(bool fromOurMain, BWAPI::UnitType unitType)
        : PlayItem("Elevator")
        , fromOurMain(fromOurMain)
        , unitType(unitType)
        , complete(false)
        , pickupPosition(BWAPI::Positions::Invalid)
        , dropPosition(BWAPI::Positions::Invalid)
        , shuttle(nullptr)
        , squad(nullptr)
        , count(0)
        , countAddedToSquad(0)
        , pickingUp(true)
{}

void Elevator::update()
{
    auto setPositions = [&]()
    {
        auto positions = selectPositions(fromOurMain ? Map::getMyMain() : Map::getEnemyStartingMain());
        if (!positions.first.isValid() || !positions.second.isValid()) return false;

        dropPosition = BWAPI::Position(fromOurMain ? positions.second : positions.first) + BWAPI::Position(16, 16);
        pickupPosition = BWAPI::Position(fromOurMain ? positions.first : positions.second) + BWAPI::Position(16, 16);
        return true;
    };

    /* Initialization when enemy base is known */
    if (!squad)
    {
        auto enemyMain = Map::getEnemyStartingMain();
        if (!enemyMain) return;

        /* Get pickup and dropoff positions, aborting if they cannot be determined */
        if (!setPositions())
        {
            status.complete = true;
            return;
        }

        squad = std::make_shared<AttackBaseSquad>(enemyMain, "Elevator");
        squad->ignoreCombatSim = !fromOurMain; // Really the squad should be allowed to reposition itself, but for now just have it always attack
        Marshal::addSquad(squad);
    }

    /* If the drop position has become unwalkable, pick a new one */
    /* This could happen if the enemy builds something there */
    if (!validElevatorPosition(BWAPI::TilePosition(dropPosition)))
    {
        if (setPositions())
        {
#if LOGGING_ENABLED
            AppLog::Get() << "Updated elevator tiles: " << dropPosition << ", " << pickupPosition;
#endif
        }
        else
        {
            complete = true;

#if LOGGING_ENABLED
            AppLog::Get() << "No longer valid elevator tiles; cancelling elevator";
#endif
        }
    }

    /* Mark the play complete when our shuttle dies */
    if (shuttle && !shuttle->exists())
    {
        complete = true;
        shuttle = nullptr;
    }

    /* Clean up and micro the transfer queue */
    for (auto iterRef = transferQueue.begin(); iterRef != transferQueue.end();)
    {
        if ((*iterRef)->exists())
        {
            /* If the unit is near the pickup position and is under attack, cancel the play */
            /* This might happen on maps where there isn't a pickup position sufficiently far away from the natural */
            if ((*iterRef)->isBeingAttacked() && (*iterRef)->prevPos.getApproxDistance(pickupPosition) < 200)
            {
                complete = true;
            }

            (*iterRef)->moveTo(pickupPosition);
            iterRef++;
        }
        else
        {
            /* Remove units from the count if they were not near the pickup position when they died */
            if ((*iterRef)->prevPos.getApproxDistance(pickupPosition) > 400)
            {
                count--;
            }

            iterRef = transferQueue.erase(iterRef);
        }
    }

    /* Move units through the sets when their state changes */
    auto move = [](std::set<OwnedUnit> &from, std::set<OwnedUnit> &to, bool loaded)
    {
        for (auto iterRef = from.begin(); iterRef != from.end();)
        {
            if ((*iterRef)->rawUnit->isLoaded() == loaded)
            {
                to.insert(*iterRef);
                iterRef = from.erase(iterRef);
            }
            else
            {
                iterRef++;
            }
        }
    };
    move(transferQueue, transferring, true);
    move(transferring, transferred, false);

    auto isTransferredUnitUnderAttack = [&]()
    {
        return std::any_of(transferred.begin(), transferred.end(), [](const auto &gameUnit)
        {
            return gameUnit->isBeingAttacked();
        });
    };

    /* Move transferred units to the squad if: */
    /* - The play is complete (no new units are being transferred) */
    /* - The squad is already attacking */
    /* - We have moved at least 8 of them */
    /* - We have no more waiting */
    /* - One of our transferred units is under attack */
    if (complete || !squad->empty() || transferred.size() > 7 || transferQueue.empty() || isTransferredUnitUnderAttack())
    {
        for (auto &gameUnit : transferred)
        {
            squad->addUnit(gameUnit);
            countAddedToSquad++;
            Opponent::incrementGameValue("elevatoredUnits");
        }
        transferred.clear();
    }

    /* Handle the complete state */
    /* Here we are just waiting until there are no units remaining that this play needs to own */
    if (complete)
    {
        /* Release all units from the transfer queue */
        status.removedUnits.insert(status.removedUnits.end(), transferQueue.begin(), transferQueue.end());
        transferQueue.clear();

        /* If units are in transit, drop them off */
        if (shuttle && !shuttle->rawUnit->getLoadedUnits().empty())
        {
            drop(shuttle, dropPosition);
            return;
        }

        /* Release the shuttle */
        if (shuttle)
        {
            status.removedUnits.push_back(shuttle);
            shuttle = nullptr;
        }

        /* Disband the play when the squad is empty or the enemy starting main is destroyed */
        if (squad->empty() || Map::getEnemyStartingMain()->owner != BWAPI::Broodwar->enemy())
        {
            status.complete = true;
        }

        return;
    }

    /* If we don't have a shuttle, the play hasn't started yet, so determine if it should */
    /* The shuttle dying triggers play completion, so execution won't get this far */
    if (!shuttle)
    {
        /* Never start the play after frame 12000 */
        if (frameNow > 12000)
        {
            complete = true;
            return;
        }

        if (!fromOurMain)
        {
            /* Wait to start the play if any of the following checks fail: */
            /* - The enemy has taken their natural behind a bunker */
            /* - We have a stable contain */
            /* - We have at least 5 dragoons */
            auto enemyNatural = Map::getEnemyStartingNatural();
            if (!Director::isEnemyContained() ||
                UnitMgr::countCompleted(unitType) < 5 ||
                !enemyNatural || enemyNatural->owner != BWAPI::Broodwar->enemy() ||
                UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Bunker) == 0)
            {
                return;
            }
        }

        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Shuttle, pickupPosition);
        return;
    }

    /* Request units */
    if (count < 12)
    {
        int span = shuttle->getDistance(pickupPosition);
        int needed = -count;
        if (span < 1280)
        {
            needed += 4;
        }
        if (span < 320)
        {
            needed += 8;
        }
        if (needed > 0)
        {
            status.unitRequirements.emplace_back(needed, unitType, pickupPosition);
        }
    }

    /* Micro the shuttle */
    /* It switches between pickup and drop */
    if (pickingUp)
    {
        if (transferring.size() == 2)
        {
            pickingUp = false;
#if CVIS_LOG_ELEVATOR_STATE
            Telemetry::log(shuttle->ident) << "Elevator: Shuttle full, switching to drop-off state";
#endif
        }
        else
        {
            OwnedUnit closestPickup = nullptr;
            int closestPickupDist = INT_MAX;
            for (auto &gameUnit : transferQueue)
            {
                int span = Pathfinder::GetGroundDistance(gameUnit->prevPos, pickupPosition, gameUnit->type);
                if (span != -1 && span < closestPickupDist)
                {
                    closestPickup = gameUnit;
                    closestPickupDist = span;
                }
            }
            if (closestPickup && closestPickupDist < 64)
            {
                pickup(shuttle, closestPickup);
#if CVIS_LOG_ELEVATOR_STATE
                Telemetry::log(shuttle->ident) << "Elevator: Loading " << (*closestPickup);
#endif
            }
            else if (!transferring.empty() && closestPickupDist > 500)
            {
                /* Transfer one unit if the next one is a long way away */
                pickingUp = false;
#if CVIS_LOG_ELEVATOR_STATE
                Telemetry::log(shuttle->ident) << "Elevator: Next unit is far away, switching to drop-off state";
#endif
            }
            else
            {
                shuttleMove(shuttle, pickupPosition);
#if CVIS_LOG_ELEVATOR_STATE
                Telemetry::log(shuttle->ident) << "Elevator: Waiting for unit to load";
#endif
            }
        }
    }
    if (!pickingUp)
    {
        if (transferring.empty() && shuttle->rawUnit->getLoadedUnits().empty())
        {
            pickingUp = true;
            shuttleMove(shuttle, pickupPosition);
#if CVIS_LOG_ELEVATOR_STATE
            Telemetry::log(shuttle->ident) << "Elevator: Shuttle empty, switching to pickup state";
#endif
        }
        else
        {
            drop(shuttle, dropPosition);
#if CVIS_LOG_ELEVATOR_STATE
            Telemetry::log(shuttle->ident) << "Elevator: Drop unit";
#endif
        }
    }

    /* Micro transferred units */
    for (auto &gameUnit : transferred)
    {
        gameUnit->moveTo(dropPosition);
    }

    /* Move to complete mode when: */
    /* - we've transferred more than 10 units and all of our sets are empty */
    /* - half or more of our transferred units have died */
    if ((count > 10 && transferQueue.empty() && transferring.empty() && transferred.empty()) ||
        (countAddedToSquad > 2 && squad->combatUnitCount() <= (countAddedToSquad / 2)))
    {
        complete = true;
    }
}

void Elevator::disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                       const std::function<void(const OwnedUnit)> &movableUnitCallback)
{
    if (shuttle) removedUnitCallback(shuttle);
    for (auto &gameUnit : transferQueue)
    {
        removedUnitCallback(gameUnit);
    }
    for (auto &gameUnit : transferred)
    {
        removedUnitCallback(gameUnit);
    }

    PlayItem::disband(removedUnitCallback, movableUnitCallback);
}

bool Elevator::canReassignUnit(const OwnedUnit &gameUnit) const
{
    /* Don't allow the shuttle to be reassigned */
    if (gameUnit == shuttle) return false;

    /* Allow other units to be reassigned only if they are in the transfer queue */
    return transferQueue.contains(gameUnit);
}

void Elevator::addUnit(const OwnedUnit &gameUnit)
{
    if (gameUnit->type == BWAPI::UnitTypes::Protoss_Shuttle)
    {
        shuttle = gameUnit;
    }
    else
    {
        transferQueue.insert(gameUnit);
        gameUnit->moveTo(pickupPosition);
        count++;
    }
}

void Elevator::removeUnit(const OwnedUnit &gameUnit)
{
    if (shuttle == gameUnit) shuttle = nullptr;
    count -= (int)transferQueue.erase(gameUnit);
    transferring.erase(gameUnit);
    transferred.erase(gameUnit);

    PlayItem::removeUnit(gameUnit);
}

std::pair<BWAPI::TilePosition, BWAPI::TilePosition> Elevator::selectPositions(Base *baseLoc)
{
    auto baseTile = BWAPI::TilePositions::Invalid;
    auto mapTile = BWAPI::TilePositions::Invalid;

    /* Get base areas */
    std::set<const BWEM::Area*> areas;
    if (baseLoc->isStartingBase())
    {
        auto &baseAreas = Map::getStartingBaseAreas(baseLoc);
        areas.insert(baseAreas.begin(), baseAreas.end());
    }
    else
    {
        areas.insert(baseLoc->getArea());
    }

    /* Get all narrow chokes out of the base areas */
    std::set<Choke*> chokes;
    for (auto &area : areas)
    {
        for (auto &bwemChoke : area->ChokePoints())
        {
            auto choke = Map::choke(bwemChoke);
            if (choke->isNarrowChoke)
            {
                chokes.insert(choke);
            }
        }
    }

    /* Now score all of the base's edge tiles based on their proximity to the depot and choke(s) */
    struct BaseTile
    {
        static bool cmp(const BaseTile &a, const BaseTile &b)
        {
            return a.score > b.score;
        }

        explicit BaseTile(BWAPI::TilePosition tilePos, const BWEM::Area *area, int score) : tilePos(tilePos), area(area), score(score) {}

        BWAPI::TilePosition tilePos;
        const BWEM::Area *area;
        int score;
    };
    std::vector<BaseTile> scoredBaseTiles;
    auto &areasToEdgePositions = Map::getAreasToEdgePositions();
    for (auto &area : areas)
    {
        auto edgePositions = areasToEdgePositions.find(area);
        if (edgePositions != areasToEdgePositions.end())
        {
            for (auto &edgePosition : edgePositions->second)
            {
                /* Must be a walkable tile with some space around it */
                if (!validElevatorPosition(edgePosition)) continue;

                auto pt = BWAPI::Position(edgePosition) + BWAPI::Position(16, 16);
                int score = pt.getApproxDistance(baseLoc->getPosition()) * (int)chokes.size() * 2;
                for (auto &choke : chokes)
                {
                    score += pt.getApproxDistance(choke->midpoint);
                }
                scoredBaseTiles.emplace_back(edgePosition, area, score);
            }
        }
    }
    std::sort(scoredBaseTiles.begin(), scoredBaseTiles.end(), BaseTile::cmp);

    /* Determine the areas to avoid when choosing a map tile */
    /* For starting bases we want to also avoid using the natural */
    std::set<const BWEM::Area *> avoidAreas;
    avoidAreas.insert(areas.begin(), areas.end());
    if (baseLoc->isStartingBase())
    {
        auto natBase = Map::getStartingBaseNatural(baseLoc);
        if (natBase)
        {
            avoidAreas.insert(natBase->getArea());
        }
    }

    /* Take the best tile that has a corresponding map tile no more than 8 tiles away */
    auto &edgePositionsToArea = Map::getEdgePositionsToArea();
    for (auto &tilePos : scoredBaseTiles)
    {
        auto spiral = Geom::Spiral();
        while (spiral.radius <= 8)
        {
            spiral.Next();
            BWAPI::TilePosition here = tilePos.tilePos + BWAPI::TilePosition(spiral.x, spiral.y);

            /* Must be a walkable tile with some space around it */
            if (!validElevatorPosition(here)) continue;

            /* Must not be on an island or leaf area */
            if (Map::isOnIsland(here)) continue;
            if (Map::isInLeafArea(here)) continue;

            /* Must be an edge tile */
            auto edgeTileArea = edgePositionsToArea.find(here);
            if (edgeTileArea == edgePositionsToArea.end()) continue;

            /* Must not be in one of our avoid areas */
            if (avoidAreas.contains(edgeTileArea->second)) continue;

            /* We have a match */
            baseTile = tilePos.tilePos;
            mapTile = here;
            goto breakOuterLoop;
        }
    }
    breakOuterLoop:;

#if CVIS_ELEVATORTILES_HEATMAP
    /* Dump to CherryVis if we found valid tiles */
    if (baseTile.isValid() && mapTile.isValid())
    {
        std::vector<long> elevatorCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight(), 0);
        elevatorCVis[baseTile.x + baseTile.y * BWAPI::Broodwar->mapWidth()] = 100;
        elevatorCVis[mapTile.x + mapTile.y * BWAPI::Broodwar->mapWidth()] = 100;

        Telemetry::addHeatmap("ElevatorTiles", elevatorCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
    }
#endif
#if LOGGING_ENABLED
    if (baseTile.isValid() && mapTile.isValid())
    {
        AppLog::Get() << "Selected elevator tiles: " << baseTile << ", " << mapTile;
    }
    else
    {
        AppLog::Get() << "Could not find elevator tiles";
    }
#endif

    return std::make_pair(baseTile, mapTile);
}
