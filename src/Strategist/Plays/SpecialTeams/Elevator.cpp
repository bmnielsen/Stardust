#include "Elevator.h"

#include "Map.h"
#include "General.h"
#include "Workers.h"
#include "Units.h"
#include "Builder.h"
#include "PathFinding.h"

/*
 * For now does a fairly hard-coded rush on two maps.
 */

Elevator::Elevator()
        : Play("Elevator")
        , complete(false)
        , shuttle(nullptr)
        , squad(std::make_shared<AttackBaseSquad>(Map::getEnemyStartingMain()))
        , count(0)
        , pickingUp(true)
{
    // Really the squad should be allowed to reposition itself, but for now just have it always attack
    squad->ignoreCombatSim = true;

    General::addSquad(squad);

    // Destination
    if (BWAPI::Broodwar->mapHash() == "4e24f217d2fe4dbfa6799bc57f74d8dc939d425b" ||
        BWAPI::Broodwar->mapHash() == "e39c1c81740a97a733d227e238bd11df734eaf96")
    {
        if (BWAPI::Broodwar->self()->getStartLocation().y > 60)
        {
            pickupPosition = BWAPI::Position(BWAPI::TilePosition(17, 28)) + BWAPI::Position(16, 16);
            dropPosition = BWAPI::Position(BWAPI::TilePosition(20, 23)) + BWAPI::Position(16, 16);
        }
        else
        {
            pickupPosition = BWAPI::Position(BWAPI::TilePosition(76, 97)) + BWAPI::Position(16, 16);
            dropPosition = BWAPI::Position(BWAPI::TilePosition(75, 102)) + BWAPI::Position(16, 16);
        }
    }

    // Heartbreak Ridge
    if (BWAPI::Broodwar->mapHash() == "6f8da3c3cc8d08d9cf882700efa049280aedca8c" ||
        BWAPI::Broodwar->mapHash() == "fe25d8b79495870ac1981c2dfee9368f543321e3" ||
        BWAPI::Broodwar->mapHash() == "d9757c0adcfd61386dff8fe3e493e9e8ef9b45e3" ||
        BWAPI::Broodwar->mapHash() == "ecb9c70c5594a5c6882baaf4857a61824fba0cfa")
    {
        if (BWAPI::Broodwar->self()->getStartLocation().x > 60)
        {
            pickupPosition = BWAPI::Position(BWAPI::TilePosition(19, 12)) + BWAPI::Position(16, 16);
            dropPosition = BWAPI::Position(BWAPI::TilePosition(21, 24)) + BWAPI::Position(16, 16);
        }
        else
        {
            pickupPosition = BWAPI::Position(BWAPI::TilePosition(108, 83)) + BWAPI::Position(16, 16);
            dropPosition = BWAPI::Position(BWAPI::TilePosition(108, 73)) + BWAPI::Position(16, 16);
        }
    }
}

void Elevator::update()
{
    // Mark the play complete when our shuttle dies
    if (shuttle && !shuttle->exists())
    {
        complete = true;
        shuttle = nullptr;
    }

    // Handle the complete state
    // Here we are just waiting until there are no units remaining that this play needs to own
    if (complete)
    {
        // Move all transferred units to the squad
        for (auto &unit: transferred)
        {
            squad->addUnit(unit);
        }
        transferred.clear();

        status.removedUnits.insert(status.removedUnits.end(), transferQueue.begin(), transferQueue.end());
        transferQueue.clear();

        // TODO: Handle case where the shuttle has units in it
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

    // Ensure we have a shuttle
    if (!shuttle)
    {
        // Start the play once we have three completed dragoons
        if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Dragoon) < 3) return;

        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Shuttle, pickupPosition);
        return;
    }

    // Clean up and micro transfer queue
    for (auto it = transferQueue.begin(); it != transferQueue.end();)
    {
        if ((*it)->exists())
        {
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

    // Request units
    // Start by rallying 4 until the shuttle is near the pickup tile
    int needed = ((shuttle->getDistance(pickupPosition) < 320) ? 15 : 4) - count;
    if (needed > 0)
    {
        status.unitRequirements.emplace_back(needed, BWAPI::UnitTypes::Protoss_Dragoon, pickupPosition);
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

    // Micro the shuttle
    // It switches between pickup and drop
    if (pickingUp)
    {
        if (transferring.size() == 2)
        {
            pickingUp = false;
        }
        else
        {
            MyUnit closestPickup = nullptr;
            int closestPickupDist = INT_MAX;
            for (auto &unit: transferQueue)
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
                shuttle->load(closestPickup->bwapiUnit);
            }
            else if (!transferring.empty() && closestPickupDist > 500)
            {
                // Transfer one unit if the next one is a long way away
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
            if (shuttle->bwapiUnit->getLastCommand().getType() != BWAPI::UnitCommandTypes::Unload_All_Position)
            {
                shuttle->unloadAll(dropPosition);
            }
        }
    }

    // Micro transferred units
    for (auto &unit: transferred)
    {
        unit->moveTo(dropPosition);
    }

    auto isTransferringUnitUnderAttack = [&]()
    {
        for (auto &unit : transferring)
        {
            if (unit->isBeingAttacked()) return true;
        }

        return false;
    };

    // Move transferred units to the squad if the shuttle is empty and either:
    // - The squad is already attacking
    // - We have moved at least 8 of them
    // - We have no more waiting
    // - One of our transferring units is under attack
    if (transferring.empty() && (!squad->empty() || transferred.size() > 7 || transferQueue.empty() || isTransferringUnitUnderAttack()))
    {
        for (auto &unit: transferred)
        {
            squad->addUnit(unit);
        }
        transferred.clear();
    }

    // Move to complete mode when all of our sets are empty
    if (count > 10 && transferQueue.empty() && transferring.empty() && transferred.empty())
    {
        complete = true;
    }
}

void Elevator::disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                       const std::function<void(const MyUnit)> &movableUnitCallback)
{
    if (shuttle) removedUnitCallback(shuttle);
    for (auto &unit: transferQueue)
    {
        removedUnitCallback(unit);
    }
    for (auto &unit: transferred)
    {
        removedUnitCallback(unit);
    }

    Play::disband(removedUnitCallback, movableUnitCallback);
}

void Elevator::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Only produces a shuttle when the requirement hasn't been met
    for (auto &unitRequirement: status.unitRequirements)
    {
        if (unitRequirement.count < 1) continue;
        if (unitRequirement.type != BWAPI::UnitTypes::Protoss_Shuttle) continue;

        prioritizedProductionGoals[PRIORITY_SPECIALTEAMS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       unitRequirement.type,
                                                                       unitRequirement.count,
                                                                       1);
    }
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
