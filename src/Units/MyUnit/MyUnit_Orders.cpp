#include "MyUnit.h"

#include "DebugFlag_UnitOrders.h"

void MyUnitImpl::move(BWAPI::Position position, bool force)
{
    if (issuedOrderThisFrame)
    {
        Log::Get() << "DUPLICATE ORDER: " << *this << ": Move to " << BWAPI::WalkPosition(position) << (force ? " (forced)" : "");
        return;
    }

    auto skipMoveCommand = [&]()
    {
        if (force) return false;
        if (bwapiUnit->isStuck()) return false;
        if (bwapiUnit->getLastCommand().getType() != BWAPI::UnitCommandTypes::Move) return false;
        if (bwapiUnit->getLastCommand().getTargetPosition().getApproxDistance(position) > 3) return false;

        // Don't resend orders to similar positions too quickly
        if (bwapiUnit->getLastCommandFrame() > (currentFrame - BWAPI::Broodwar->getLatencyFrames() - 3)) return false;

        // Don't resend order if the unit is moving
        return bwapiUnit->getLastCommandFrame() < frameLastMoved;
    };

    if (skipMoveCommand()) return;

    issuedOrderThisFrame = bwapiUnit->move(position);
    if (issuedOrderThisFrame)
    {
        lastMoveFrame = currentFrame;
    }

#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "Order: Move to " << BWAPI::WalkPosition(position) << (force ? " (forced)" : "");
#endif
}

void MyUnitImpl::attack(BWAPI::Unit target, bool force)
{
    if (!target || !target->exists())
    {
        Log::Get() << "ATTACK INVALID TARGET: " << *this;
        return;
    }
    if (issuedOrderThisFrame)
    {
        Log::Get() << "DUPLICATE ORDER: " << *this << ": Attack " << target->getType() << " @ " << BWAPI::WalkPosition(target->getPosition());
        return;
    }

    BWAPI::UnitCommand currentCommand(bwapiUnit->getLastCommand());
    if (!force &&
        !bwapiUnit->isStuck() &&
        currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
        currentCommand.getTarget() == target)
    {
        return;
    }

    issuedOrderThisFrame = bwapiUnit->attack(target);

#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "Order: Attack " << target->getType() << " @ " << BWAPI::WalkPosition(target->getPosition());
#endif
}

void MyUnitImpl::attackMove(BWAPI::Position position)
{
    if (issuedOrderThisFrame)
    {
        Log::Get() << "DUPLICATE ORDER: " << *this << ": Attack move to " << BWAPI::WalkPosition(position);
        return;
    }

    BWAPI::UnitCommand currentCommand(bwapiUnit->getLastCommand());
    if (!bwapiUnit->isStuck() &&
        currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Move &&
        currentCommand.getTargetPosition() == position)
    {
        return;
    }

    issuedOrderThisFrame = bwapiUnit->attack(position);

#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "Order: Attack move to " << BWAPI::WalkPosition(position);
#endif
}

void MyUnitImpl::rightClick(BWAPI::Unit target)
{
    if (!target || !target->exists())
    {
        Log::Get() << "RIGHT-CLICK INVALID TARGET: " << *this;
        return;
    }
    if (issuedOrderThisFrame)
    {
        Log::Get() << "DUPLICATE ORDER: " << *this << ": Right-click " << target->getType() << " @ " << BWAPI::WalkPosition(target->getPosition());
        return;
    }

    // Unless is it a mineral field, don't click on the same target again
    if (!target->getType().isMineralField())
    {
        BWAPI::UnitCommand currentCommand(bwapiUnit->getLastCommand());
        if (currentCommand.getType() == BWAPI::UnitCommandTypes::Right_Click_Unit &&
            currentCommand.getTargetPosition() == target->getPosition())
        {
            return;
        }
    }

    issuedOrderThisFrame = bwapiUnit->rightClick(target);

#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "Order: Right-click " << target->getType() << " @ " << BWAPI::WalkPosition(target->getPosition());
#endif
}

void MyUnitImpl::gather(BWAPI::Unit target)
{
    if (!target || !target->exists())
    {
        Log::Get() << "GATHER INVALID TARGET: " << *this;
        return;
    }
    if (issuedOrderThisFrame)
    {
        Log::Get() << "DUPLICATE ORDER: " << *this << ": Gather " << target->getType() << " @ " << BWAPI::WalkPosition(target->getPosition());
        return;
    }

    // Unless it is a mineral field, don't gather on the same target again
    if (!target->getType().isMineralField())
    {
        BWAPI::UnitCommand currentCommand(bwapiUnit->getLastCommand());
        if (currentCommand.getType() == BWAPI::UnitCommandTypes::Gather &&
            currentCommand.getTargetPosition() == target->getPosition())
        {
            return;
        }
    }

    issuedOrderThisFrame = bwapiUnit->gather(target);

#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "Order: Gather " << target->getType() << " @ " << BWAPI::WalkPosition(target->getPosition());
#endif
}

void MyUnitImpl::returnCargo()
{
    if (issuedOrderThisFrame)
    {
        Log::Get() << "DUPLICATE ORDER: " << *this << ": Return cargo";
        return;
    }

    issuedOrderThisFrame = bwapiUnit->returnCargo();

#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "Order: Return cargo";
#endif
}

bool MyUnitImpl::build(BWAPI::UnitType type, BWAPI::TilePosition tile)
{
    if (!tile.isValid())
    {
        Log::Get() << "BUILD INVALID TILE: " << *this;
        return false;
    }
    if (issuedOrderThisFrame)
    {
        Log::Get() << "DUPLICATE ORDER: " << *this << ": Build " << type << " @ " << BWAPI::WalkPosition(tile);
        return false;
    }

    issuedOrderThisFrame = bwapiUnit->build(type, tile);

#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "Order: Build " << type << " @ " << BWAPI::WalkPosition(tile);
#endif

    return issuedOrderThisFrame;
}

bool MyUnitImpl::train(BWAPI::UnitType type)
{
    if (issuedOrderThisFrame)
    {
        Log::Get() << "DUPLICATE ORDER: " << *this << ": Train " << type;
        return false;
    }

    issuedOrderThisFrame = bwapiUnit->train(type);

#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "Order: Train " << type;
#endif

    return issuedOrderThisFrame;
}

bool MyUnitImpl::upgrade(BWAPI::UpgradeType type)
{
    if (issuedOrderThisFrame)
    {
        Log::Get() << "DUPLICATE ORDER: " << *this << ": Upgrade " << type;
        return false;
    }

    issuedOrderThisFrame = bwapiUnit->upgrade(type);

#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "Order: Upgrade " << type;
#endif

    return issuedOrderThisFrame;
}

bool MyUnitImpl::research(BWAPI::TechType type)
{
    if (issuedOrderThisFrame)
    {
        Log::Get() << "DUPLICATE ORDER: " << *this << ": Research " << type;
        return false;
    }

    issuedOrderThisFrame = bwapiUnit->research(type);

#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "Order: Research " << type;
#endif

    return issuedOrderThisFrame;
}

void MyUnitImpl::stop()
{
    if (issuedOrderThisFrame)
    {
        Log::Get() << "DUPLICATE ORDER: " << *this << ": Stop";
        return;
    }

    issuedOrderThisFrame = bwapiUnit->stop();

#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "Order: Stop";
#endif
}

void MyUnitImpl::cancelConstruction()
{
    if (issuedOrderThisFrame)
    {
        Log::Get() << "DUPLICATE ORDER: " << *this << ": Cancel Construction";
        return;
    }

    issuedOrderThisFrame = bwapiUnit->cancelConstruction();

#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "Order: Cancel Construction";
#endif
}

void MyUnitImpl::load(BWAPI::Unit cargo)
{
    if (issuedOrderThisFrame)
    {
        Log::Get() << "DUPLICATE ORDER: " << *this << ": Load " << cargo->getType() << " @ " << cargo->getTilePosition();
        return;
    }

    // Don't re-issue the same command
    BWAPI::UnitCommand currentCommand(bwapiUnit->getLastCommand());
    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Load &&
        currentCommand.getTarget() == cargo)
    {
        return;
    }

    issuedOrderThisFrame = bwapiUnit->load(cargo);

#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "Order: Load " << cargo->getType() << " @ " << cargo->getTilePosition();
#endif
}

void MyUnitImpl::unloadAll(BWAPI::Position pos)
{
    if (issuedOrderThisFrame)
    {
        Log::Get() << "DUPLICATE ORDER: " << *this << ": Unload All";
        return;
    }

    issuedOrderThisFrame = bwapiUnit->unloadAll(pos);

#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "Order: Unload All " << BWAPI::WalkPosition(pos);
#endif
}
