#include "ElevatorRush.h"

#include "Map.h"
#include "General.h"
#include "Workers.h"
#include "Units.h"
#include "Builder.h"
#include "PathFinding.h"

/*
 * For now does a fairly hard-coded rush on two maps.
 */

ElevatorRush::ElevatorRush()
        : PlayItem("Elevator Rush")
        , complete(false)
        , builder(nullptr)
        , shuttle(nullptr)
        , squad(std::make_shared<AttackBaseSquad>(Map::getEnemyStartingMain(), "ElevatorRush"))
        , count(0)
        , pickingUp(true)
{
    /* Really the squad should be allowed to reposition itself, but for now just have it always attack */
    squad->ignoreCombatSim = true;

    Marshal::addSquad(squad);

    /* Destination */
    if (BWAPI::Broodwar->mapHash() == "4e24f217d2fe4dbfa6799bc57f74d8dc939d425b" ||
        BWAPI::Broodwar->mapHash() == "e39c1c81740a97a733d227e238bd11df734eaf96")
    {
        if (BWAPI::Broodwar->self()->getStartLocation().y > 60)
        {
            pylonTile = BWAPI::TilePosition(11, 30);
            roboTile = BWAPI::TilePosition(13, 30);
            pickupPosition = BWAPI::Position(BWAPI::TilePosition(17, 28)) + BWAPI::Position(16, 16);
            dropPosition = BWAPI::Position(BWAPI::TilePosition(20, 23)) + BWAPI::Position(16, 16);
        }
        else
        {
            pylonTile = BWAPI::TilePosition(83, 97);
            roboTile = BWAPI::TilePosition(80, 97);
            pickupPosition = BWAPI::Position(BWAPI::TilePosition(76, 97)) + BWAPI::Position(16, 16);
            dropPosition = BWAPI::Position(BWAPI::TilePosition(75, 102)) + BWAPI::Position(16, 16);
        }
    }

    /* Heartbreak Ridge */
    if (BWAPI::Broodwar->mapHash() == "6f8da3c3cc8d08d9cf882700efa049280aedca8c" ||
        BWAPI::Broodwar->mapHash() == "fe25d8b79495870ac1981c2dfee9368f543321e3" ||
        BWAPI::Broodwar->mapHash() == "d9757c0adcfd61386dff8fe3e493e9e8ef9b45e3" ||
        BWAPI::Broodwar->mapHash() == "ecb9c70c5594a5c6882baaf4857a61824fba0cfa")
    {
        if (BWAPI::Broodwar->self()->getStartLocation().x > 60)
        {
            pylonTile = BWAPI::TilePosition(15, 8);
            roboTile = BWAPI::TilePosition(15, 10);
            pickupPosition = BWAPI::Position(BWAPI::TilePosition(19, 12)) + BWAPI::Position(16, 16);
            dropPosition = BWAPI::Position(BWAPI::TilePosition(21, 24)) + BWAPI::Position(16, 16);
        }
        else
        {
            pylonTile = BWAPI::TilePosition(111, 86);
            roboTile = BWAPI::TilePosition(110, 84);
            pickupPosition = BWAPI::Position(BWAPI::TilePosition(108, 83)) + BWAPI::Position(16, 16);
            dropPosition = BWAPI::Position(BWAPI::TilePosition(108, 73)) + BWAPI::Position(16, 16);
        }
    }
}

void ElevatorRush::update()
{
    /* Mark the play complete when our shuttle dies */
    if (shuttle && !shuttle->exists())
    {
        complete = true;
        shuttle = nullptr;
    }

    /* Handle the complete state */
    /* Here we are just waiting until there are no units remaining that this play needs to own */
    if (complete)
    {
        if (builder)
        {
            WorkerMgr::releaseWorker(builder);
            builder = nullptr;
        }

        /* Move all transferred units to the squad */
        for (auto &gameUnit : transferred)
        {
            squad->addUnit(gameUnit);
        }
        transferred.clear();

        status.removedUnits.insert(status.removedUnits.end(), transferQueue.begin(), transferQueue.end());
        transferQueue.clear();

        /* TODO: Handle case where the shuttle has units in it */
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

    /* First check if we're finished building */
    auto robo = UnitMgr::myBuildingAt(roboTile);
    if (!robo)
    {
        /* Start the play once we have two completed dragoons */
        if (UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Dragoon) < 2) return;

        /* Make sure we have a builder */
        if (builder && !builder->exists()) builder = nullptr;
        if (!builder)
        {
            builder = WorkerMgr::getClosestReassignableWorker(BWAPI::Position(pylonTile) + BWAPI::Position(16, 16),
                                                            false);
            if (!builder) return;
            WorkerMgr::reserveWorker(builder);
        }

        /* Build the appropriate building */
        auto pylon = UnitMgr::myBuildingAt(pylonTile);
        if (!pylon)
        {
            Constructor::build(BWAPI::UnitTypes::Protoss_Pylon, pylonTile, builder);
        }
        else
        {
            Constructor::build(BWAPI::UnitTypes::Protoss_Robotics_Facility, roboTile, builder);
        }

        return;
    }

    /* We don't need the builder once the robo is started */
    if (builder)
    {
        WorkerMgr::releaseWorker(builder);
        builder = nullptr;
    }

    /* Wait until our robo is finished */
    if (!robo->completed) return;

    /* Ensure we have a shuttle */
    if (!shuttle)
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Shuttle, pickupPosition);
    }

    /* Clean up and micro transfer queue */
    for (auto iterRef = transferQueue.begin(); iterRef != transferQueue.end();)
    {
        if ((*iterRef)->exists())
        {
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

    /* Request units */
    /* Start by rallying 4 until the shuttle is complete */
    int needed = (shuttle ? 15 : 4) - count;
    if (needed > 0)
    {
        status.unitRequirements.emplace_back(needed, BWAPI::UnitTypes::Protoss_Dragoon, pickupPosition);
    }

    if (!shuttle) return;

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

    /* Micro the shuttle */
    /* It switches between pickup and drop */
    if (pickingUp)
    {
        if (transferring.size() == 2)
        {
            pickingUp = false;
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
                shuttle->load(closestPickup->rawUnit);
            }
            else if (!transferring.empty() && closestPickupDist > 500)
            {
                /* Transfer one unit if the next one is a long way away */
                pickingUp = false;
            }
            else
            {
                shuttle->moveTo(pickupPosition);
            }
        }
    }
    if (!pickingUp)
    {
        if (transferring.empty())
        {
            pickingUp = true;
            shuttle->moveTo(pickupPosition);
        }
        else
        {
            if (shuttle->rawUnit->getLastCommand().getType() != BWAPI::UnitCommandTypes::Unload_All_Position)
            {
                shuttle->unloadAll(dropPosition);
            }
        }
    }

    /* Micro transferred units */
    for (auto &gameUnit : transferred)
    {
        gameUnit->moveTo(dropPosition);
    }

    /* Move transferred units to the squad if the shuttle is empty and either: */
    /* - The squad is already attacking */
    /* - We have moved at least 8 of them */
    /* - We have no more waiting */
    /* TODO attack if any of our transferred units are under attack? */
    if (transferring.empty() && (!squad->empty() || transferred.size() > 7 || transferQueue.empty()))
    {
        for (auto &gameUnit : transferred)
        {
            squad->addUnit(gameUnit);
        }
        transferred.clear();
    }

    /* Move to complete mode when all of our sets are empty */
    if (transferQueue.empty() && transferring.empty() && transferred.empty())
    {
        complete = true;
    }
}

void ElevatorRush::disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                           const std::function<void(const OwnedUnit)> &movableUnitCallback)
{
    if (builder) WorkerMgr::releaseWorker(builder);
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

void ElevatorRush::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    /* Only produces a shuttle when the requirement hasn't been met */
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.count < 1) continue;
        if (unitRequirement.type != BWAPI::UnitTypes::Protoss_Shuttle) continue;

        prioProdGoals[PRIORITY_SPECIALTEAMS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       label,
                                                                       unitRequirement.type,
                                                                       unitRequirement.count,
                                                                       1);
    }
}

void ElevatorRush::addUnit(const OwnedUnit &gameUnit)
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

void ElevatorRush::removeUnit(const OwnedUnit &gameUnit)
{
    if (shuttle == gameUnit) shuttle = nullptr;
    count -= (int)transferQueue.erase(gameUnit);
    transferring.erase(gameUnit);
    transferred.erase(gameUnit);

    PlayItem::removeUnit(gameUnit);
}
