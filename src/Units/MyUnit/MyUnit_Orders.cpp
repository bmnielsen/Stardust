#include "MyUnit.h"
#include "MyWorker.h"

#include "DebugFlag_UnitOrders.h"

void MyUnitImpl::move(BWAPI::Position position, bool force)
{
    if (issuedOrderThisFrame)
    {
        AppLog::Get() << "DUPLICATE ORDER: " << *this << ": Move to " << BWAPI::WalkPosition(position) << (force ? " (forced)" : "");
        return;
    }

    auto skipMoveCommand = [&]()
    {
        if (force) return false;
        if (rawUnit->isStuck()) return false;
        if (rawUnit->getLastCommand().getType() != BWAPI::UnitCommandTypes::Move) return false;
        if (rawUnit->getLastCommand().getTargetPosition().getApproxDistance(position) > 3) return false;

        /* Don't resend orders to similar positions too quickly */
        if (lastCommandFrame > (frameNow - BWAPI::Broodwar->getLatencyFrames() - 3)) return false;

        /* Don't resend order if the unit is moving */
        return lastCommandFrame < frameLastMoved;
    };

    if (skipMoveCommand()) return;

    issuedOrderThisFrame = rawUnit->move(position);
    if (issuedOrderThisFrame)
    {
        lastMoveFrame = frameNow;
    }

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Order: Move to " << BWAPI::WalkPosition(position) << (force ? " (forced)" : "");
#endif
}

void MyUnitImpl::attack(BWAPI::Unit target, bool force)
{
    if (!target || !target->exists())
    {
        AppLog::Get() << "ATTACK INVALID TARGET: " << *this;
        return;
    }
    if (issuedOrderThisFrame)
    {
        AppLog::Get() << "DUPLICATE ORDER: " << *this << ": Attack " << target->getType() << " @ " << BWAPI::WalkPosition(target->getPosition());
        return;
    }

    BWAPI::UnitCommand currentCommand(rawUnit->getLastCommand());
    if (!force &&
        !rawUnit->isStuck() &&
        currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
        currentCommand.getTarget() == target)
    {
        return;
    }

    issuedOrderThisFrame = rawUnit->attack(target);

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Order: Attack " << target->getType() << " @ " << BWAPI::WalkPosition(target->getPosition());
#endif
}

void MyUnitImpl::attackMove(BWAPI::Position position)
{
    if (issuedOrderThisFrame)
    {
        AppLog::Get() << "DUPLICATE ORDER: " << *this << ": Attack move to " << BWAPI::WalkPosition(position);
        return;
    }

    BWAPI::UnitCommand currentCommand(rawUnit->getLastCommand());
    if (!rawUnit->isStuck() &&
        currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Move &&
        currentCommand.getTargetPosition() == position)
    {
        return;
    }

    issuedOrderThisFrame = rawUnit->attack(position);

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Order: Attack move to " << BWAPI::WalkPosition(position);
#endif
}

void MyUnitImpl::rightClick(BWAPI::Unit target)
{
    if (!target || !target->exists())
    {
        AppLog::Get() << "RIGHT-CLICK INVALID TARGET: " << *this;
        return;
    }
    if (issuedOrderThisFrame)
    {
        AppLog::Get() << "DUPLICATE ORDER: " << *this << ": Right-click " << target->getType() << " @ " << BWAPI::WalkPosition(target->getPosition());
        return;
    }

    /* Unless is it a mineral field, don't click on the same target again */
    if (!target->getType().isMineralField())
    {
        BWAPI::UnitCommand currentCommand(rawUnit->getLastCommand());
        if (currentCommand.getType() == BWAPI::UnitCommandTypes::Right_Click_Unit &&
            currentCommand.getTargetPosition() == target->getPosition())
        {
            return;
        }
    }

    issuedOrderThisFrame = rawUnit->rightClick(target);

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Order: Right-click " << target->getType() << " @ " << BWAPI::WalkPosition(target->getPosition());
#endif
}

bool MyWorkerImpl::gather(BWAPI::Unit target)
{
    if (!target || !target->exists())
    {
        AppLog::Get() << "GATHER INVALID TARGET: " << *this;
        return false;
    }
    if (issuedOrderThisFrame)
    {
        AppLog::Get() << "DUPLICATE ORDER: " << *this << ": Gather " << target->getType() << " @ " << BWAPI::WalkPosition(target->getPosition());
        return false;
    }

    /* Unless it is a mineral field, don't gather on the same target again */
    if (!target->getType().isMineralField())
    {
        BWAPI::UnitCommand currentCommand(rawUnit->getLastCommand());
        if (currentCommand.getType() == BWAPI::UnitCommandTypes::Gather &&
            currentCommand.getTargetPosition() == target->getPosition())
        {
            return false;
        }
    }

    auto outcome = rawUnit->gather(target);
    if (outcome)
    {
        issuedOrderThisFrame = true;
        gatherCommandFrames.insert(frameNow);
    }

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Order: Gather " << target->getType() << " @ " << BWAPI::WalkPosition(target->getPosition());
#endif

    return outcome;
}

bool MyWorkerImpl::returnCargo()
{
    if (issuedOrderThisFrame)
    {
        AppLog::Get() << "DUPLICATE ORDER: " << *this << ": Return cargo";
        return false;
    }

    auto outcome = rawUnit->returnCargo();
    issuedOrderThisFrame |= outcome;

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Order: Return cargo";
#endif

    return outcome;
}

bool MyUnitImpl::build(BWAPI::UnitType type, BWAPI::TilePosition tilePos)
{
    if (!tilePos.isValid())
    {
        AppLog::Get() << "BUILD INVALID TILE: " << *this;
        return false;
    }
    if (issuedOrderThisFrame)
    {
        AppLog::Get() << "DUPLICATE ORDER: " << *this << ": Build " << type << " @ " << BWAPI::WalkPosition(tilePos);
        return false;
    }

    issuedOrderThisFrame = rawUnit->build(type, tilePos);

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Order: Build " << type << " @ " << BWAPI::WalkPosition(tilePos);
#endif

    return issuedOrderThisFrame;
}

bool MyUnitImpl::train(BWAPI::UnitType type)
{
    if (issuedOrderThisFrame)
    {
        AppLog::Get() << "DUPLICATE ORDER: " << *this << ": Train " << type;
        return false;
    }

    issuedOrderThisFrame = rawUnit->train(type);

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Order: Train " << type;
#endif

    return issuedOrderThisFrame;
}

bool MyUnitImpl::upgrade(BWAPI::UpgradeType type)
{
    if (issuedOrderThisFrame)
    {
        AppLog::Get() << "DUPLICATE ORDER: " << *this << ": Upgrade " << type;
        return false;
    }

    issuedOrderThisFrame = rawUnit->upgrade(type);

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Order: Upgrade " << type;
#endif

    return issuedOrderThisFrame;
}

bool MyUnitImpl::research(BWAPI::TechType type)
{
    if (issuedOrderThisFrame)
    {
        AppLog::Get() << "DUPLICATE ORDER: " << *this << ": Research " << type;
        return false;
    }

    issuedOrderThisFrame = rawUnit->research(type);

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Order: Research " << type;
#endif

    return issuedOrderThisFrame;
}

void MyUnitImpl::stop()
{
    if (issuedOrderThisFrame)
    {
        AppLog::Get() << "DUPLICATE ORDER: " << *this << ": Stop";
        return;
    }

    issuedOrderThisFrame = rawUnit->stop();

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Order: Stop";
#endif
}

void MyUnitImpl::cancelConstruction()
{
    if (issuedOrderThisFrame)
    {
        AppLog::Get() << "DUPLICATE ORDER: " << *this << ": Cancel Construction";
        return;
    }

    issuedOrderThisFrame = rawUnit->cancelConstruction();

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Order: Cancel Construction";
#endif
}

void MyUnitImpl::load(BWAPI::Unit cargo)
{
    if (issuedOrderThisFrame)
    {
        AppLog::Get() << "DUPLICATE ORDER: " << *this << ": Load " << cargo->getType() << " @ " << cargo->getTilePosition();
        return;
    }

    /* Don't re-issue the same command */
    BWAPI::UnitCommand currentCommand(rawUnit->getLastCommand());
    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Load &&
        currentCommand.getTarget() == cargo)
    {
        return;
    }

    issuedOrderThisFrame = rawUnit->load(cargo);

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Order: Load " << cargo->getType() << " @ " << cargo->getTilePosition();
#endif
}

void MyUnitImpl::unloadAll(BWAPI::Position pt)
{
    if (issuedOrderThisFrame)
    {
        AppLog::Get() << "DUPLICATE ORDER: " << *this << ": Unload All";
        return;
    }

    issuedOrderThisFrame = rawUnit->unloadAll(pt);

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Order: Unload All " << BWAPI::WalkPosition(pt);
#endif
}

void MyUnitImpl::unload(BWAPI::Unit cargo)
{
    if (issuedOrderThisFrame)
    {
        AppLog::Get() << "DUPLICATE ORDER: " << *this << ": Unload " << cargo->getType() << " @ " << cargo->getTilePosition();
        return;
    }

    issuedOrderThisFrame = rawUnit->unload(cargo);

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Order: Unload " << cargo->getType() << " @ " << cargo->getTilePosition();
#endif
}

void MyUnitImpl::setProducerRallyPosition(BWAPI::Position pt) const
{
    if (!producer) return;

    auto current = producer->getRallyPosition();
    if (current.isValid() && pt.getApproxDistance(current) < 32) return;

    producer->setRallyPoint(pt);

#if DEBUG_UNIT_ORDERS
    Telemetry::log(producer->getID()) << "Order: Set rally position to " << BWAPI::WalkPosition(pt);
#endif
}
