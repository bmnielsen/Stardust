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
        , unstickUntil(-1)
        , frameLastMoved(0)
{
}

std::ostream &operator<<(std::ostream &os, const MyUnitImpl &unit)
{
    os << unit.type << ":" << unit.id << "@" << BWAPI::WalkPosition(unit.getTilePosition());
    return os;
}

void MyUnitImpl::update(BWAPI::Unit unit)
{
    if (!unit || !unit->exists()) return;

    if (unit->getPosition() != lastPosition) frameLastMoved = BWAPI::Broodwar->getFrameCount();

    UnitImpl::update(unit);

    issuedOrderThisFrame = false;
    moveCommand = nullptr;

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

bool MyUnitImpl::isBeingManufacturedOrCarried() const
{
    if (!exists()) return false;

    // For Protoss, anything incomplete that isn't a building is currently being manufactured
    // Will need to change if we ever support building Zerg units
    if (!bwapiUnit->isCompleted() && !bwapiUnit->getType().isBuilding()) return true;

    // Otherwise look at whether it is loaded
    return bwapiUnit->isLoaded();
}

void MyUnitImpl::attackUnit(const Unit &target, std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets, bool clusterAttacking)
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
    if (unstickUntil > BWAPI::Broodwar->getFrameCount())
    {
#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Unstick pending until " << unstickUntil;
#endif

        return true;
    }

    // If the unit is listed as stuck, send a stop command unless we have done so recently
    if (bwapiUnit->isStuck() && unstickUntil < (BWAPI::Broodwar->getFrameCount() - 10))
    {
#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Sending stop command to unstick";
#endif

        stop();
        unstickUntil = BWAPI::Broodwar->getFrameCount() + BWAPI::Broodwar->getRemainingLatencyFrames();
        return true;
    }

    return false;
}
