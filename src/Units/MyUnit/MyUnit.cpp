#include "MyUnit.h"

#include "UnitUtil.h"
#include "Units.h"
#include "Geo.h"
#include "Players.h"

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
        , simulatedPositionsUpdated(false)
{
    recentCommands.resize(BWAPI::Broodwar->getLatencyFrames() + 1, unit->getLastCommand());
    simulatedPositions.resize(BWAPI::Broodwar->getLatencyFrames() + 1, unit->getPosition());
    simulatedHeading.resize(BWAPI::Broodwar->getLatencyFrames() + 1, 0);
}

std::ostream &operator<<(std::ostream &os, const MyUnitImpl &unit)
{
    os << unit.type << ":" << unit.id << "@" << BWAPI::WalkPosition(unit.getTilePosition());
    return os;
}

void MyUnitImpl::update(BWAPI::Unit unit)
{
    if (!unit || !unit->exists()) return;

    // Update command positions
    recentCommands.pop_front();
    recentCommands.emplace_back(unit->getLastCommand());
    simulatedPositionsUpdated = false;

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

    auto myPredictedPosition = predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames());
    auto targetPredictedPosition = target->predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames());
    auto predictedDist = Geo::EdgeToEdgeDistance(type, myPredictedPosition, target->type, targetPredictedPosition);
    auto rangeToTarget = range(target);

    // Predicted to be in range or target is stationary: attack
    if (predictedDist <= rangeToTarget || targetPredictedPosition == target->lastPosition)
    {
        // Re-send the attack command when we are coming into range
        bool forceAttackCommand = false;
        if (dist > rangeToTarget && predictedDist <= rangeToTarget && cooldownUntil < (currentFrame + BWAPI::Broodwar->getRemainingLatencyFrames())
            && bwapiUnit->getLastCommand().type == BWAPI::UnitCommandTypes::Attack_Unit)
        {
            // If we recently sent the attack command, check our predicted distance when it will kick in
            // If we still expect to be in range, we don't need to do anything
            int lastCommandTakesEffect = lastCommandFrame + BWAPI::Broodwar->getLatencyFrames() - currentFrame;
            if (lastCommandTakesEffect >= 0)
            {
                auto distAtAttackFrame =
                        (lastCommandTakesEffect == 0)
                            ? dist
                            : Geo::EdgeToEdgeDistance(type, predictPosition(lastCommandTakesEffect),
                                                      target->type, target->predictPosition(lastCommandTakesEffect));
                if (distAtAttackFrame <= rangeToTarget)
                {
#if DEBUG_UNIT_ORDERS
                    CherryVis::log(id) << "Attack: Attack command effective @ " << lastCommandTakesEffect
                        << "; dist=" << distAtAttackFrame << "; don't touch";
#endif
                    return;
                }

#if DEBUG_UNIT_ORDERS
                CherryVis::log(id) << "Attack: Attack command effective @ " << lastCommandTakesEffect
                                   << "; dist=" << distAtAttackFrame << "; resend attack command";
#endif
            }
            else
            {
#if DEBUG_UNIT_ORDERS
                CherryVis::log(id) << "Attack: Attack command effective @ " << lastCommandTakesEffect
                                   << "; resending attack command";
#endif
            }

            // Otherwise force the attack command
            forceAttackCommand = true;
        }

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

BWAPI::Position MyUnitImpl::simulatePosition(int frames) const
{
    if (!simulatedPositionsUpdated) updateSimulatedPositions();

    return simulatedPositions[frames - 1];
}

int MyUnitImpl::simulateHeading(int frames) const
{
    if (!simulatedPositionsUpdated) updateSimulatedPositions();

    return simulatedHeading[frames - 1];
}

void MyUnitImpl::updateSimulatedPositions() const
{
    simulatedPositionsUpdated = true;

    int x = lastPosition.x;
    int bwX = x << 8;
    int y = lastPosition.y;
    int bwY = y << 8;

    int heading = BWHeading();

    int speed = BWSpeed();
    double topSpeed = Players::unitTopSpeed(player, type);
    int bwTopSpeed = (int)(topSpeed * 256.0);
    if (speed > bwTopSpeed) speed = bwTopSpeed;
    int acceleration = UnitUtil::Acceleration(type, topSpeed);

    for (int i=0; i<recentCommands.size(); i++)
    {
        // Compute desired heading to move target at this frame
        int desiredHeading;
        auto moveTarget = recentCommands[i].getTargetPosition();
        if (moveTarget.isValid())
        {
            desiredHeading = Geo::BWDirection({moveTarget.x - x, moveTarget.y - y});
        }
        else
        {
            desiredHeading = heading;
        }

        // Simulate movement
        Geo::BWMovement(bwX, bwY, heading, desiredHeading, type.turnRadius(), speed, acceleration, bwTopSpeed);

        // Assign simulated position and heading
        x = bwX >> 8;
        y = bwY >> 8;
        simulatedPositions[i].x = x;
        simulatedPositions[i].y = y;
        simulatedHeading[i] = heading;
    }
}
