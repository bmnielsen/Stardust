#include <Players/Players.h>
#include "MyUnit.h"

#include "UnitUtil.h"
#include "Units.h"
#include "Geo.h"

#include "DebugFlag_UnitOrders.h"

MyUnitImpl::MyUnitImpl(BWAPI::Unit unit)
        : UnitImpl(unit)
        , producer(nullptr)
        , energy(unit->getEnergy())
        , lastCastFrame(-1)
        , issuedOrderThisFrame(false)
        , moveCommand(nullptr)
        , targetPosition(BWAPI::Positions::Invalid)
        , currentlyMovingTowards(BWAPI::Positions::Invalid)
        , grid(nullptr)
        , gridNode(nullptr)
        , lastMoveFrame(0)
        , unstickUntil(-1)
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

    if (bwapiUnit->isCompleted()) producer = nullptr;

    if (energy - unit->getEnergy() > 45)
    {
        lastCastFrame = currentFrame;
        Log::Get() << "Unit cast spell: " << *this;
    }
    energy = unit->getEnergy();

    // If this unit has just gone on cooldown, add an upcoming attack on its target
    if (bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit ||
        bwapiUnit->getOrder() == BWAPI::Orders::AttackUnit)
    {
        auto cooldown = std::max(unit->getGroundWeaponCooldown(), unit->getAirWeaponCooldown());
        if (cooldown > 0 && cooldown > (cooldownUntil - currentFrame + 1))
        {
            auto target = Units::get(
                    (bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit)
                    ? bwapiUnit->getLastCommand().getTarget()
                    : bwapiUnit->getOrderTarget());
            if (target)
            {
                target->addUpcomingAttack(Units::get(bwapiUnit));
            }
        }
    }

    UnitImpl::update(unit);

    issuedOrderThisFrame = false;
    moveCommand = nullptr;

    // Guard against buildings having a deep training queue
    if (BWAPI::Broodwar->self()->supplyUsed() < 380 &&
        unit->getLastCommand().getType() != BWAPI::UnitCommandTypes::Cancel_Train &&
        unit->getLastCommand().getType() != BWAPI::UnitCommandTypes::Cancel_Train_Slot &&
        unit->getTrainingQueue().size() > 1 &&
        lastCommandFrame < (currentFrame - BWAPI::Broodwar->getLatencyFrames()))
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

    // Cancel dying incomplete buildings
    if (type.isBuilding() &&
        !completed &&
        bwapiUnit->isUnderAttack() &&
        bwapiUnit->canCancelConstruction() &&
        (lastShields + lastHealth) < 20)
    {
        Log::Get() << "Cancelling dying " << type << " @ " << getTilePosition();
        bwapiUnit->cancelConstruction();
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

void MyUnitImpl::attackUnit(const Unit &target,
                            std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                            bool clusterAttacking,
                            int enemyAoeRadius)
{
    // If the enemy is a long way away, move to it
    int dist = getDistance(target);
    if (dist > 320 || !target->bwapiUnit->isVisible())
    {
#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Attack: Moving to target @ " << BWAPI::WalkPosition(target->lastPosition);
#endif
        moveTo(target->lastPosition);
        return;
    }

    // Determine if we should force the attack command
    // We do this if there is a high likelihood we are moving backwards because of pathing issues
    bool forceAttackCommand = false;
    auto predictedPosition = predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames() + 2);
    if (!predictedPosition.isValid()) predictedPosition = lastPosition;
    if (lastCommandFrame < (currentFrame - BWAPI::Broodwar->getLatencyFrames() - 6))
    {
        forceAttackCommand = Geo::EdgeToEdgeDistance(type, predictedPosition, target->type, target->lastPosition) > dist;
    }

    // If the target is already in range, just attack it
    int myRange = range(target);
    if (dist <= myRange)
    {
        attack(target->bwapiUnit, forceAttackCommand);
        return;
    }

    // If the target is predicted to be in range shortly, attack it
    auto predictedTargetPosition = target->predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames() + 2);
    if (!predictedTargetPosition.isValid()) predictedTargetPosition = target->lastPosition;
    auto predictedDist = Geo::EdgeToEdgeDistance(type, predictedPosition, target->type, predictedTargetPosition);
    if (predictedDist <= myRange)
    {
        attack(target->bwapiUnit, forceAttackCommand);
        return;
    }

    // If the target is stationary or is close and moving towards us, attack it
    if (predictedTargetPosition == target->lastPosition || (predictedDist <= dist && predictedDist < std::min(myRange + 64, 128)))
    {
        attack(target->bwapiUnit, forceAttackCommand);
        return;
    }

    // Plot an intercept course
    auto interceptPosition = intercept(target);
    if (!interceptPosition.isValid()) interceptPosition = target->predictPosition(BWAPI::Broodwar->getLatencyFrames() + 2);
    if (!interceptPosition.isValid()) interceptPosition = target->lastPosition;

#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "Attack: Moving to intercept @ " << BWAPI::WalkPosition(interceptPosition);
#endif
    moveTo(interceptPosition);
}

bool MyUnitImpl::isReady() const
{
    // When we have a large army, only micro units every other frame to avoid having commands dropped
    if (BWAPI::Broodwar->self()->supplyUsed() > 250 &&
        (currentFrame % 2) != (id % 2))
    {
        return false;
    }

    return !immobile;
}

bool MyUnitImpl::unstick()
{
    // If we recently sent a command meant to unstick the unit, give it a bit of time to kick in
    if (unstickUntil > currentFrame)
    {
#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Unstick pending until " << unstickUntil;
#endif

        return true;
    }

    // If the unit is listed as stuck, send a stop command unless we have done so recently
    if (bwapiUnit->isStuck() && unstickUntil < (currentFrame - 10))
    {
#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Sending stop command to unstick";
#endif

        stop();
        unstickUntil = currentFrame + BWAPI::Broodwar->getRemainingLatencyFrames();
        return true;
    }

    return false;
}
