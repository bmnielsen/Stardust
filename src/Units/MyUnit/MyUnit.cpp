#include <Players/Players.h>
#include "MyUnit.h"

#include "UnitUtil.h"
#include "Geo.h"
#include "Map.h"

MyUnitImpl::MyUnitImpl(BWAPI::Unit unit)
        : UnitImpl(unit)
        , issuedOrderThisFrame(false)
        , moveCommand(nullptr)
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
    moveCommand = nullptr;

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

    return false;
}
