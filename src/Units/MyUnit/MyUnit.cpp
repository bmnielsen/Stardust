#include "MyUnit.h"

#include "UnitUtil.h"
#include "Units.h"
#include "Geo.h"
#include "Players.h"

#include "DebugFlag_UnitOrders.h"

#define DEBUG_ORDER_PROCESS_INDEX false

namespace
{
    /* BWAPI's isVisible is really intended for knowing if an enemy unit is in the fog or not */
    /* We want to also know if a unit is loaded or harvesting gas, as these remove the unit from the game's visible units list */
    bool isVisible(BWAPI::Unit gameUnit)
    {
        if (!gameUnit->getType().isBuilding() && !gameUnit->isCompleted()) return false;
        if (!gameUnit->isVisible()) return false;
        if (gameUnit->isLoaded()) return false;
        if (gameUnit->getOrder() == BWAPI::Orders::HarvestGas && gameUnit->getOrderTimer() > 0) return false;
        return true;
    }
}

MyUnitImpl::MyUnitImpl(BWAPI::Unit gameUnit)
        : UnitImpl(gameUnit)
        , orderProcessIndex(isVisible(gameUnit) ? (10 + frameNow) : -1)
        , producer(nullptr)
        , energy(gameUnit->getEnergy())
        , lastCastFrame(-1)
        , completionFrame(gameUnit->isCompleted() ? frameNow : -1)
        , issuedOrderThisFrame(false)
        , moveCommand(nullptr)
        , targetPosition(BWAPI::Positions::Invalid)
        , currentlyMovingTowards(BWAPI::Positions::Invalid)
        , grid(nullptr)
        , gridNode(nullptr)
        , lastMoveFrame(0)
        , unstickUntil(-1)
        , simulatedPositionsUpdated(false)
        , visible(isVisible(gameUnit))
{
    recentCommands.resize(BWAPI::Broodwar->getLatencyFrames() + 1, gameUnit->getLastCommand());
    simulatedPositions.resize(BWAPI::Broodwar->getLatencyFrames() + 1, gameUnit->getPosition());
    simulatedHeading.resize(BWAPI::Broodwar->getLatencyFrames() + 1, 0);
#if DEBUG_ORDER_PROCESS_INDEX
    Telemetry::log(ident) << "Order process index: " << orderProcessIndex;
#endif
}

std::ostream &operator<<(std::ostream &os, const MyUnitImpl &gameUnit)
{
    os << gameUnit.type << ":" << gameUnit.ident << "@" << BWAPI::WalkPosition(gameUnit.getTilePosition());
    return os;
}

void MyUnitImpl::update(BWAPI::Unit gameUnit)
{
    if (!gameUnit || !gameUnit->exists()) return;

    /* Update the order process index if the unit's visibility has changed */
    if (isVisible(gameUnit) != visible)
    {
        if (visible)
        {
            visible = false;
            orderProcessIndex = -1;
#if DEBUG_ORDER_PROCESS_INDEX
            Telemetry::log(ident) << "Order process index reset as unit has become invisible";
#endif
        }
        else
        {
            visible = true;
            orderProcessIndex = 10 + frameNow;
            orderProcessTimer = 0;
#if DEBUG_ORDER_PROCESS_INDEX
            Telemetry::log(ident) << "Order process index: " << orderProcessIndex;
#endif
        }
    }

    /* Update command positions */
    recentCommands.pop_front();
    recentCommands.emplace_back(gameUnit->getLastCommand());
    if (recentCommands.back().target)
    {
        recentCommands.back().assignTarget(recentCommands.back().target->getPosition());
    }
    simulatedPositionsUpdated = false;

    if (rawUnit->isCompleted() && completionFrame == -1)
    {
        producer = nullptr;
        completionFrame = frameNow;
    }

    if (energy - gameUnit->getEnergy() > 45)
    {
        lastCastFrame = frameNow;
        AppLog::Get() << "Unit cast spell: " << *this;
    }
    energy = gameUnit->getEnergy();

    /* If this unit has just gone on cooldown, add an upcoming attack on its target */
    if (rawUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit ||
        rawUnit->getOrder() == BWAPI::Orders::AttackUnit)
    {
        auto cooldown = std::max(gameUnit->getGroundWeaponCooldown(), gameUnit->getAirWeaponCooldown());
        if (cooldown > 0 && cooldown > (cooldownUntil - frameNow + 1))
        {
            auto target = UnitMgr::get(
                    (rawUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit)
                    ? rawUnit->getLastCommand().getTarget()
                    : rawUnit->getOrderTarget());
            if (target)
            {
                target->addUpcomingAttack(UnitMgr::get(rawUnit));
            }
        }
    }

    UnitImpl::update(gameUnit);

    issuedOrderThisFrame = false;
    moveCommand = nullptr;

    /* Guard against buildings having a deep training queue */
    /* Disabled in tournament builds as it causes crashes in BWAPI because of https://github.com/bwapi/bwapi/issues/864 */
#if LOGGING_ENABLED
    if (BWAPI::Broodwar->self()->supplyUsed() < 380 &&
        gameUnit->getLastCommand().getType() != BWAPI::UnitCommandTypes::Cancel_Train &&
        gameUnit->getLastCommand().getType() != BWAPI::UnitCommandTypes::Cancel_Train_Slot &&
        gameUnit->getTrainingQueue().size() > 1 &&
        lastCommandFrame < (frameNow - BWAPI::Broodwar->getLatencyFrames()))
    {
        AppLog::Get() << "WARNING: Training queue for " << gameUnit->getType() << " @ " << gameUnit->getTilePosition()
                   << " is too deep! Cancelling later units.";
        std::ostringstream units;
        for (int i = 0; i < gameUnit->getTrainingQueue().size(); i++)
        {
            if (i > 0) units << ",";
            units << gameUnit->getTrainingQueue()[i];
        }
        AppLog::Get() << "Queue: " << units.str();
        gameUnit->cancelTrain(1);
    }
#endif

    /* Cancel dying incomplete buildings */
    if (type.isBuilding() &&
        !completed &&
        rawUnit->isUnderAttack() &&
        rawUnit->canCancelConstruction() &&
        (lastShields + lastHealth) < 20)
    {
        AppLog::Get() << "Cancelling dying " << type << " @ " << getTilePosition();
        rawUnit->cancelConstruction();
    }
}

bool MyUnitImpl::isBeingManufacturedOrCarried() const
{
    if (!exists()) return false;

    /* For Protoss, anything incomplete that isn't a building is currently being manufactured */
    /* Will need to change if we ever support building Zerg units */
    if (!rawUnit->isCompleted() && !rawUnit->getType().isBuilding()) return true;

    /* Otherwise look at whether it is loaded */
    return rawUnit->isLoaded();
}

void MyUnitImpl::attackUnit(const Unit &target,
                            std::vector<std::pair<OwnedUnit, Unit>> &unitsAndTargets,
                            bool clusterAttacking,
                            int enemyAoeRadius)
{
    /* If the enemy is a long way away, move to it */
    int span = getDistance(target);
    if (span > 320 || !target->rawUnit->isVisible())
    {
#if DEBUG_UNIT_ORDERS
        Telemetry::log(ident) << "Attack: Moving to target @ " << BWAPI::WalkPosition(target->simPosition);
#endif
        moveTo(target->simPosition);
        return;
    }

    auto myPredictedPosition = predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames());
    auto targetPredictedPosition = target->predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames());
    auto predictedDist = Geom::EdgeToEdgeDistance(type, myPredictedPosition, target->type, targetPredictedPosition);
    auto rangeToTarget = range(target);

    /* Predicted to be in range or target is stationary: attack */
    if (predictedDist <= rangeToTarget || targetPredictedPosition == target->simPosition)
    {
        /* Re-send the attack command when we are coming into range */
        bool forceAttackCommand = false;
        if (span > rangeToTarget && predictedDist <= rangeToTarget && cooldownUntil < (frameNow + BWAPI::Broodwar->getRemainingLatencyFrames())
            && rawUnit->getLastCommand().type == BWAPI::UnitCommandTypes::Attack_Unit)
        {
            /* If we recently sent the attack command, check our predicted distance when it will kick in */
            /* If we still expect to be in range, we don't need to do anything */
            int lastCommandTakesEffect = lastCommandFrame + BWAPI::Broodwar->getLatencyFrames() - frameNow;
            if (lastCommandTakesEffect >= 0)
            {
                auto distAtAttackFrame =
                        (lastCommandTakesEffect == 0)
                            ? span
                            : Geom::EdgeToEdgeDistance(type, predictPosition(lastCommandTakesEffect),
                                                      target->type, target->predictPosition(lastCommandTakesEffect));
                if (distAtAttackFrame <= rangeToTarget)
                {
#if DEBUG_UNIT_ORDERS
                    Telemetry::log(ident) << "Attack: Attack command effective @ " << lastCommandTakesEffect
                        << "; dist=" << distAtAttackFrame << "; don't touch";
#endif
                    return;
                }

#if DEBUG_UNIT_ORDERS
                Telemetry::log(ident) << "Attack: Attack command effective @ " << lastCommandTakesEffect
                                   << "; dist=" << distAtAttackFrame << "; resend attack command";
#endif
            }
            else
            {
#if DEBUG_UNIT_ORDERS
                Telemetry::log(ident) << "Attack: Attack command effective @ " << lastCommandTakesEffect
                                   << "; resending attack command";
#endif
            }

            /* Otherwise force the attack command */
            forceAttackCommand = true;
        }

        attack(target->rawUnit, forceAttackCommand);
        return;
    }

    /* Plot an intercept course */
    auto interceptPosition = intercept(target);
    if (!interceptPosition.isValid()) interceptPosition = target->predictPosition(BWAPI::Broodwar->getLatencyFrames() + 2);

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Attack: Moving to intercept @ " << BWAPI::WalkPosition(interceptPosition);
#endif
    moveTo(interceptPosition);
}

bool MyUnitImpl::isReady() const
{
    /* When we have a large army, only micro units every other frame to avoid having commands dropped */
    if (BWAPI::Broodwar->self()->supplyUsed() > 250 &&
        (frameNow % 2) != (ident % 2))
    {
        return false;
    }

    return !immobile;
}

bool MyUnitImpl::unstick()
{
    /* Unit is dead */
    if (!rawUnit) return true;

    /* If we recently sent a command meant to unstick the unit, give it a bit of time to kick in */
    if (unstickUntil > frameNow)
    {
#if DEBUG_UNIT_ORDERS
        Telemetry::log(ident) << "Unstick pending until " << unstickUntil;
#endif

        return true;
    }

    /* If the unit is listed as stuck, send a stop command unless we have done so recently */
    if (rawUnit->isStuck() && unstickUntil < (frameNow - 10))
    {
#if DEBUG_UNIT_ORDERS
        Telemetry::log(ident) << "Sending stop command to unstick";
#endif

        stop();
        unstickUntil = frameNow + BWAPI::Broodwar->getRemainingLatencyFrames();
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

    int x = prevPos.x;
    int bwX = x << 8;
    int y = prevPos.y;
    int bwY = y << 8;

    int heading = BWHeading();

    int speed = BWSpeed();
    double topSpeed = PlayerInfo::unitTopSpeed(player, type);
    int bwTopSpeed = PlayerInfo::unitBWTopSpeed(player, type);
    if (speed > bwTopSpeed) speed = bwTopSpeed;
    int acceleration = UnitHelper::Acceleration(type, topSpeed);
    if (!rawUnit->isAccelerating()) acceleration *= -1;

    for (int i=0; i<recentCommands.size(); i++)
    {
        /* Compute desired heading to move target at this frame */
        int desiredHeading;
        if (recentCommands[i].type == BWAPI::UnitCommandTypes::Move && recentCommands[i].getTargetPosition().isValid())
        {
            desiredHeading = Geom::BWDirection({recentCommands[i].getTargetPosition().x - x, recentCommands[i].getTargetPosition().y - y});
        }
        else
        {
            desiredHeading = heading;
        }

        /* Simulate movement */
        Geom::BWMovement(bwX, bwY, heading, desiredHeading, type.turnRadius(), speed, acceleration, bwTopSpeed);

        /* Assign simulated position and heading */
        x = bwX >> 8;
        y = bwY >> 8;
        simulatedPositions[i].x = x;
        simulatedPositions[i].y = y;
        simulatedHeading[i] = heading;
    }
}
