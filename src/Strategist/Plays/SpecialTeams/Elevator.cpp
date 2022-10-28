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

namespace
{
    const int SHUTTLE_HALT_DISTANCE = UnitUtil::HaltDistance(BWAPI::UnitTypes::Protoss_Shuttle) + 16;

    bool validElevatorPosition(BWAPI::TilePosition tile)
    {
        auto validAndWalkable = [](BWAPI::TilePosition tile)
        {
            return tile.isValid() && Map::isWalkable(tile);
        };

        // The tile itself must be valid and walkable
        if (!validAndWalkable(tile)) return false;

        // At least three of its direct neighbours must be valid and walkable
        int count = 0;
        if (validAndWalkable(tile + BWAPI::TilePosition(1, 0))) count++;
        if (validAndWalkable(tile + BWAPI::TilePosition(-1, 0))) count++;
        if (validAndWalkable(tile + BWAPI::TilePosition(0, 1))) count++;
        if (validAndWalkable(tile + BWAPI::TilePosition(0, -1))) count++;
        return count >= 3;
    }

    BWAPI::Position scaledPosition(BWAPI::Position currentPosition, BWAPI::Position vector, int length)
    {
        auto scaledVector = Geo::ScaleVector(vector, length);
        if (scaledVector == BWAPI::Positions::Invalid) return BWAPI::Positions::Invalid;

        return currentPosition + scaledVector;
    }

    void shuttleMove(MyUnit &shuttle, BWAPI::Position pos)
    {
        // Always move towards a position at least halt distance away
        auto moveTarget = scaledPosition(shuttle->lastPosition, pos - shuttle->lastPosition, SHUTTLE_HALT_DISTANCE);

        // If it is invalid, it means we are exactly on our target position, so just use our main until we get somewhere else
        if (moveTarget == BWAPI::Positions::Invalid) moveTarget = Map::getMyMain()->getPosition();

        // If it is off the map, move directly to the target
        if (!moveTarget.isValid()) moveTarget = pos;

        shuttle->moveTo(moveTarget);
    }

    void drop(MyUnit &shuttle, BWAPI::Position pos)
    {
        if (shuttle->bwapiUnit->getLoadedUnits().empty()) return;

        if (shuttle->bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Unload)
        {
            // Wait 12 frames after ordering an unload
            if (shuttle->lastCommandFrame < (currentFrame - 12)) return;

            // Move to the drop position on the 12th frame
            if (shuttle->lastCommandFrame == (currentFrame - 12))
            {
                shuttleMove(shuttle, pos);
                return;
            }
        }

        // If we are close enough to the target position, start to drop
        // Otherwise move towards the position
        if (shuttle->getDistance(pos) < 32)
        {
            shuttle->unload(*(shuttle->bwapiUnit->getLoadedUnits().begin()));
        }
        else
        {
            shuttleMove(shuttle, pos);
        }
    }

    void pickup(MyUnit &shuttle, MyUnit &cargo)
    {
        if (shuttle->bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Load)
        {
            // Wait 12 frames after ordering a load
            if (shuttle->lastCommandFrame < (currentFrame - 12)) return;

            // Move to the load position on the 12th frame
            if (shuttle->lastCommandFrame == (currentFrame - 12))
            {
                shuttleMove(shuttle, cargo->lastPosition);
                return;
            }
        }

        // If we are close enough to the cargo, start to load
        // Otherwise move towards its position
        if (shuttle->getDistance(cargo) < 12)
        {
            shuttle->load(cargo->bwapiUnit);
        }
        else
        {
            shuttleMove(shuttle, cargo->lastPosition);
        }
    }
}

Elevator::Elevator(bool fromOurMain, BWAPI::UnitType unitType)
        : Play("Elevator")
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

    // Initialization when enemy base is known
    if (!squad)
    {
        auto enemyMain = Map::getEnemyStartingMain();
        if (!enemyMain) return;

        // Get pickup and dropoff positions, aborting if they cannot be determined
        if (!setPositions())
        {
            status.complete = true;
            return;
        }

        squad = std::make_shared<AttackBaseSquad>(enemyMain, "Elevator");
        squad->ignoreCombatSim = !fromOurMain; // Really the squad should be allowed to reposition itself, but for now just have it always attack
        General::addSquad(squad);
    }

    // If the drop position has become unwalkable, pick a new one
    // This could happen if the enemy builds something there
    if (!validElevatorPosition(BWAPI::TilePosition(dropPosition)))
    {
        if (setPositions())
        {
#if INSTRUMENTATION_ENABLED
            Log::Get() << "Updated elevator tiles: " << dropPosition << ", " << pickupPosition;
#endif
        }
        else
        {
            complete = true;

#if INSTRUMENTATION_ENABLED
            Log::Get() << "No longer valid elevator tiles; cancelling elevator";
#endif
        }
    }

    // Mark the play complete when our shuttle dies
    if (shuttle && !shuttle->exists())
    {
        complete = true;
        shuttle = nullptr;
    }

    // Clean up and micro the transfer queue
    for (auto it = transferQueue.begin(); it != transferQueue.end();)
    {
        if ((*it)->exists())
        {
            // If the unit is near the pickup position and is under attack, cancel the play
            // This might happen on maps where there isn't a pickup position sufficiently far away from the natural
            if ((*it)->isBeingAttacked() && (*it)->lastPosition.getApproxDistance(pickupPosition) < 200)
            {
                complete = true;
            }

            (*it)->moveTo(pickupPosition);
            it++;
        }
        else
        {
            // Remove units from the count if they were not near the pickup position when they died
            if ((*it)->lastPosition.getApproxDistance(pickupPosition) > 400)
            {
                count--;
            }

            it = transferQueue.erase(it);
        }
    }

    // Move units through the sets when their state changes
    auto move = [](std::set<MyUnit> &from, std::set<MyUnit> &to, bool loaded)
    {
        for (auto it = from.begin(); it != from.end();)
        {
            if ((*it)->bwapiUnit->isLoaded() == loaded)
            {
                to.insert(*it);
                it = from.erase(it);
            }
            else
            {
                it++;
            }
        }
    };
    move(transferQueue, transferring, true);
    move(transferring, transferred, false);

    auto isTransferredUnitUnderAttack = [&]()
    {
        for (auto &unit : transferred)
        {
            if (unit->isBeingAttacked()) return true;
        }

        return false;
    };

    // Move transferred units to the squad if:
    // - The play is complete (no new units are being transferred)
    // - The squad is already attacking
    // - We have moved at least 8 of them
    // - We have no more waiting
    // - One of our transferred units is under attack
    if (complete || !squad->empty() || transferred.size() > 7 || transferQueue.empty() || isTransferredUnitUnderAttack())
    {
        for (auto &unit : transferred)
        {
            squad->addUnit(unit);
            countAddedToSquad++;
            Opponent::incrementGameValue("elevatoredUnits");
        }
        transferred.clear();
    }

    // Handle the complete state
    // Here we are just waiting until there are no units remaining that this play needs to own
    if (complete)
    {
        // Release all units from the transfer queue
        status.removedUnits.insert(status.removedUnits.end(), transferQueue.begin(), transferQueue.end());
        transferQueue.clear();

        // If units are in transit, drop them off
        if (shuttle && !shuttle->bwapiUnit->getLoadedUnits().empty())
        {
            drop(shuttle, dropPosition);
            return;
        }

        // Release the shuttle
        if (shuttle)
        {
            status.removedUnits.push_back(shuttle);
            shuttle = nullptr;
        }

        // Disband the play when the squad is empty or the enemy starting main is destroyed
        if (squad->empty() || Map::getEnemyStartingMain()->owner != BWAPI::Broodwar->enemy())
        {
            status.complete = true;
        }

        return;
    }

    // If we don't have a shuttle, the play hasn't started yet, so determine if it should
    // The shuttle dying triggers play completion, so execution won't get this far
    if (!shuttle)
    {
        // Never start the play after frame 12000
        if (currentFrame > 12000)
        {
            complete = true;
            return;
        }

        if (!fromOurMain)
        {
            // Wait to start the play if any of the following checks fail:
            // - The enemy has taken their natural behind a bunker
            // - We have a stable contain
            // - We have at least 5 dragoons
            auto enemyNatural = Map::getEnemyStartingNatural();
            if (!Strategist::isEnemyContained() ||
                Units::countCompleted(unitType) < 5 ||
                !enemyNatural || enemyNatural->owner != BWAPI::Broodwar->enemy() ||
                Units::countEnemy(BWAPI::UnitTypes::Terran_Bunker) == 0)
            {
                return;
            }
        }

        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Shuttle, pickupPosition);
        return;
    }

    // Request units
    if (count < 12)
    {
        int dist = shuttle->getDistance(pickupPosition);
        int needed = -count;
        if (dist < 1280)
        {
            needed += 4;
        }
        if (dist < 320)
        {
            needed += 8;
        }
        if (needed > 0)
        {
            status.unitRequirements.emplace_back(needed, unitType, pickupPosition);
        }
    }

    // Micro the shuttle
    // It switches between pickup and drop
    if (pickingUp)
    {
        if (transferring.size() == 2)
        {
            pickingUp = false;
#if CHERRYVIS_ENABLED
            CherryVis::log(shuttle->id) << "Elevator: Shuttle full, switching to drop-off state";
#endif
        }
        else
        {
            MyUnit closestPickup = nullptr;
            int closestPickupDist = INT_MAX;
            for (auto &unit : transferQueue)
            {
                int dist = PathFinding::GetGroundDistance(unit->lastPosition, pickupPosition, unit->type);
                if (dist != -1 && dist < closestPickupDist)
                {
                    closestPickup = unit;
                    closestPickupDist = dist;
                }
            }
            if (closestPickup && closestPickupDist < 64)
            {
                pickup(shuttle, closestPickup);
#if CHERRYVIS_ENABLED
                CherryVis::log(shuttle->id) << "Elevator: Loading " << (*closestPickup);
#endif
            }
            else if (!transferring.empty() && closestPickupDist > 500)
            {
                // Transfer one unit if the next one is a long way away
                pickingUp = false;
#if CHERRYVIS_ENABLED
                CherryVis::log(shuttle->id) << "Elevator: Next unit is far away, switching to drop-off state";
#endif
            }
            else
            {
                shuttleMove(shuttle, pickupPosition);
#if CHERRYVIS_ENABLED
                CherryVis::log(shuttle->id) << "Elevator: Waiting for unit to load";
#endif
            }
        }
    }
    if (!pickingUp)
    {
        if (transferring.empty() && shuttle->bwapiUnit->getLoadedUnits().empty())
        {
            pickingUp = true;
            shuttleMove(shuttle, pickupPosition);
#if CHERRYVIS_ENABLED
            CherryVis::log(shuttle->id) << "Elevator: Shuttle empty, switching to pickup state";
#endif
        }
        else
        {
            drop(shuttle, dropPosition);
#if CHERRYVIS_ENABLED
            CherryVis::log(shuttle->id) << "Elevator: Drop unit";
#endif
        }
    }

    // Micro transferred units
    for (auto &unit : transferred)
    {
        unit->moveTo(dropPosition);
    }

    // Move to complete mode when:
    // - we've transferred more than 10 units and all of our sets are empty
    // - half or more of our transferred units have died
    if ((count > 10 && transferQueue.empty() && transferring.empty() && transferred.empty()) ||
        (countAddedToSquad > 2 && squad->combatUnitCount() <= (countAddedToSquad / 2)))
    {
        complete = true;
    }
}

void Elevator::disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                       const std::function<void(const MyUnit)> &movableUnitCallback)
{
    if (shuttle) removedUnitCallback(shuttle);
    for (auto &unit : transferQueue)
    {
        removedUnitCallback(unit);
    }
    for (auto &unit : transferred)
    {
        removedUnitCallback(unit);
    }

    Play::disband(removedUnitCallback, movableUnitCallback);
}

bool Elevator::canReassignUnit(const MyUnit &unit) const
{
    // Don't allow the shuttle to be reassigned
    if (unit == shuttle) return false;

    // Allow other units to be reassigned only if they are in the transfer queue
    return transferQueue.find(unit) != transferQueue.end();
}

void Elevator::addUnit(const MyUnit &unit)
{
    if (unit->type == BWAPI::UnitTypes::Protoss_Shuttle)
    {
        shuttle = unit;
    }
    else
    {
        transferQueue.insert(unit);
        unit->moveTo(pickupPosition);
        count++;
    }
}

void Elevator::removeUnit(const MyUnit &unit)
{
    if (shuttle == unit) shuttle = nullptr;
    count -= transferQueue.erase(unit);
    transferring.erase(unit);
    transferred.erase(unit);

    Play::removeUnit(unit);
}

std::pair<BWAPI::TilePosition, BWAPI::TilePosition> Elevator::selectPositions(Base *base)
{
    auto baseTile = BWAPI::TilePositions::Invalid;
    auto mapTile = BWAPI::TilePositions::Invalid;

    // Get base areas
    std::set<const BWEM::Area*> areas;
    if (base->isStartingBase())
    {
        auto &baseAreas = Map::getStartingBaseAreas(base);
        areas.insert(baseAreas.begin(), baseAreas.end());
    }
    else
    {
        areas.insert(base->getArea());
    }

    // Get all narrow chokes out of the base areas
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

    // Now score all of the base's edge tiles based on their proximity to the depot and choke(s)
    struct BaseTile
    {
        static bool cmp(const BaseTile &a, const BaseTile &b)
        {
            return a.score > b.score;
        }

        explicit BaseTile(BWAPI::TilePosition tile, const BWEM::Area *area, int score) : tile(tile), area(area), score(score) {}

        BWAPI::TilePosition tile;
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
                // Must be a walkable tile with some space around it
                if (!validElevatorPosition(edgePosition)) continue;

                auto pos = BWAPI::Position(edgePosition) + BWAPI::Position(16, 16);
                int score = pos.getApproxDistance(base->getPosition()) * chokes.size() * 2;
                for (auto &choke : chokes)
                {
                    score += pos.getApproxDistance(choke->center);
                }
                scoredBaseTiles.emplace_back(edgePosition, area, score);
            }
        }
    }
    std::sort(scoredBaseTiles.begin(), scoredBaseTiles.end(), BaseTile::cmp);

    // Determine the areas to avoid when choosing a map tile
    // For starting bases we want to also avoid using the natural
    std::set<const BWEM::Area *> avoidAreas;
    avoidAreas.insert(areas.begin(), areas.end());
    if (base->isStartingBase())
    {
        auto natural = Map::getStartingBaseNatural(base);
        if (natural)
        {
            avoidAreas.insert(natural->getArea());
        }
    }

    // Take the best tile that has a corresponding map tile no more than 8 tiles away
    auto &edgePositionsToArea = Map::getEdgePositionsToArea();
    for (auto &tile : scoredBaseTiles)
    {
        auto spiral = Geo::Spiral();
        while (spiral.radius <= 8)
        {
            spiral.Next();
            BWAPI::TilePosition here = tile.tile + BWAPI::TilePosition(spiral.x, spiral.y);

            // Must be a walkable tile with some space around it
            if (!validElevatorPosition(here)) continue;

            // Must not be on an island or leaf area
            if (Map::isOnIsland(here)) continue;
            if (Map::isInLeafArea(here)) continue;

            // Must be an edge tile
            auto edgeTileArea = edgePositionsToArea.find(here);
            if (edgeTileArea == edgePositionsToArea.end()) continue;

            // Must not be in one of our avoid areas
            if (avoidAreas.find(edgeTileArea->second) != avoidAreas.end()) continue;

            // We have a match
            baseTile = tile.tile;
            mapTile = here;
            goto breakOuterLoop;
        }
    }
    breakOuterLoop:;

#if CHERRYVIS_ENABLED
    // Dump to CherryVis if we found valid tiles
    if (baseTile.isValid() && mapTile.isValid())
    {
        std::vector<long> elevatorCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight(), 0);
        elevatorCVis[baseTile.x + baseTile.y * BWAPI::Broodwar->mapWidth()] = 100;
        elevatorCVis[mapTile.x + mapTile.y * BWAPI::Broodwar->mapWidth()] = 100;

        CherryVis::addHeatmap("ElevatorTiles", elevatorCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
    }
#endif
#if INSTRUMENTATION_ENABLED
    if (baseTile.isValid() && mapTile.isValid())
    {
        Log::Get() << "Selected elevator tiles: " << baseTile << ", " << mapTile;
    }
    else
    {
        Log::Get() << "Could not find elevator tiles";
    }
#endif

    return std::make_pair(baseTile, mapTile);
}
