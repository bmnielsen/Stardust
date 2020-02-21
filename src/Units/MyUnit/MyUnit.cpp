#include <Players/Players.h>
#include "MyUnit.h"

#include "UnitUtil.h"
#include "Geo.h"
#include "Map.h"

namespace
{
    bool isNextToUnwalkableTerrain(BWAPI::TilePosition pos)
    {
        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                auto here = BWAPI::TilePosition(x, y);
                if (here.isValid() && !Map::isWalkable(here)) return true;
            }
        }

        return false;
    }
}

MyUnitImpl::MyUnitImpl(BWAPI::Unit unit)
        : UnitImpl(unit)
        , issuedOrderThisFrame(false)
        , targetPosition(BWAPI::Positions::Invalid)
        , currentlyMovingTowards(BWAPI::Positions::Invalid)
        , grid(nullptr)
        , gridNode(nullptr)
        , lastMoveFrame(0)
        , lastUnstickFrame(0)
{
}

std::ostream &operator<<(std::ostream &os, const MyUnitImpl &unit)
{
    os << unit.type << ":" << unit.id << "@" << BWAPI::WalkPosition(unit.getTilePosition());
    return os;
}

void MyUnitImpl::update(BWAPI::Unit unit)
{
    UnitImpl::update(unit);

    if (!unit || !unit->exists()) return;

    issuedOrderThisFrame = false;

    typeSpecificUpdate();

    // Guard against buildings having a deep training queue
    if (unit->getTrainingQueue().size() > 1 && unit->getLastCommandFrame() < (BWAPI::Broodwar->getFrameCount() - BWAPI::Broodwar->getLatencyFrames()))
    {
        Log::Get() << "WARNING: Training queue for " << unit->getType() << " @ " << unit->getTilePosition()
                   << " is too deep! Cancelling later units.";
        std::ostringstream units;
        for (int i = 0; i < unit->getTrainingQueue().size(); i++)
        {
            if (i > 0) units << ",";
            units << unit->getTrainingQueue()[i];
        }
        Log::Get() << "Queue: " << units.str();
        unit->cancelTrain(1);
    }
}

void MyUnitImpl::attackUnit(Unit target)
{
    int cooldown = target->isFlying ? bwapiUnit->getAirWeaponCooldown() : bwapiUnit->getGroundWeaponCooldown();
    int range = Players::weaponRange(player, getWeapon(target));

    // If the target is already in range, just attack it
    if (bwapiUnit->getDistance(target->bwapiUnit) <= range)
    {
        attack(target->bwapiUnit);
        return;
    }

    // Otherwise attack when our cooldown is almost over and we expect to be in range shortly
    if (cooldown <= BWAPI::Broodwar->getRemainingLatencyFrames() + 2)
    {
        auto predictedPosition = predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames() + 2);
        auto predictedTargetPosition = target->predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames() + 2);
        auto predictedDist = Geo::EdgeToEdgeDistance(type, predictedPosition, target->type, predictedTargetPosition);

        if (predictedDist <= range)
        {
            attack(target->bwapiUnit);
            return;
        }
    }

    // Plot an intercept course
    auto interceptPosition = intercept(target);
    if (!interceptPosition.isValid()) interceptPosition = target->predictPosition(5);
    move(interceptPosition);
}

bool MyUnitImpl::unstick()
{
    // If we recently sent a command meant to unstick the unit, give it a bit of time to kick in
    if (BWAPI::Broodwar->getFrameCount() - lastUnstickFrame < BWAPI::Broodwar->getLatencyFrames())
    {
        return true;
    }

    // Check if the unit is stuck on some kind of unwalkable building or terrain
    // Condition is:
    // - The last command is a valid move command
    // - The last command was issued more than 3+LF frames ago
    // - The unit is more than 32 pixels from the move target
    // - The unit is not moving
    // - The unit is next to unwalkable terrain
    BWAPI::UnitCommand currentCommand(bwapiUnit->getLastCommand());
    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Move && currentCommand.getTargetPosition().isValid()
        && (BWAPI::Broodwar->getFrameCount() - bwapiUnit->getLastCommandFrame()) >= (BWAPI::Broodwar->getLatencyFrames() + 3)
        && bwapiUnit->getDistance(currentCommand.getTargetPosition()) > 32
        && (!bwapiUnit->isMoving() || (abs(bwapiUnit->getVelocityX()) < 0.001 && abs(bwapiUnit->getVelocityY()) < 0.001))
        && isNextToUnwalkableTerrain(getTilePosition()))
    {
        // Scores the distance from a neighbouring tile to the target position
        BWAPI::Position best = BWAPI::Positions::Invalid;
        int bestDist = INT_MAX;
        auto scoreTile = [&currentCommand, &best, &bestDist](BWAPI::TilePosition tile)
        {
            if (!tile.isValid()) return;
            if (!Map::isWalkable(tile)) return;

            auto position = BWAPI::Position(tile) + BWAPI::Position(16, 16);
            int dist = currentCommand.getTargetPosition().getApproxDistance(position);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = position;
            }
        };

        auto currentTile = getTilePosition();
        scoreTile(currentTile + BWAPI::TilePosition(1, 0));
        scoreTile(currentTile + BWAPI::TilePosition(-1, 0));
        scoreTile(currentTile + BWAPI::TilePosition(0, 1));
        scoreTile(currentTile + BWAPI::TilePosition(0, -1));

        if (best.isValid())
        {
#ifdef DEBUG_UNIT_ORDERS
            CherryVis::log(id) << "Unstick by moving to neighbouring walkable tile";
#endif
            move(best);
            lastUnstickFrame = BWAPI::Broodwar->getFrameCount();
            return true;
        }

        Log::Get() << *this << " likely stuck on terrain, but no valid unstick move location could be found";
    }

    if (bwapiUnit->isStuck())
    {
        stop();
        return true;
    }

    return false;
}
