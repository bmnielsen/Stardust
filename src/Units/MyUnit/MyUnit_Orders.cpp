#include "MyUnit.h"

void MyUnit::move(BWAPI::Position position, bool force)
{
    if (issuedOrderThisFrame) return;
    if (!position.isValid()) return;

    BWAPI::UnitCommand currentCommand(unit->getLastCommand());
    if (!force &&
        !unit->isStuck() &&
        currentCommand.getType() == BWAPI::UnitCommandTypes::Move &&
        currentCommand.getTargetPosition() == position &&
        unit->isMoving())
    {
        return;
    }

    issuedOrderThisFrame = unit->move(position);

#if DEBUG_UNIT_ORDERS
    CherryVis::log(unit) << "Order: Move to " << BWAPI::WalkPosition(position) << (force ? " (forced)" : "");
#endif
}

void MyUnit::attack(BWAPI::Unit target)
{
    if (issuedOrderThisFrame) return;
    if (!target || !target->exists()) return;

    BWAPI::UnitCommand currentCommand(unit->getLastCommand());
    if (!unit->isStuck() &&
        currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
        currentCommand.getTarget() == target)
    {
        return;
    }

    issuedOrderThisFrame = unit->attack(target);

#if DEBUG_UNIT_ORDERS
    CherryVis::log(unit) << "Order: Attack " << target->getType() << " @ " << BWAPI::WalkPosition(target->getPosition());
#endif
}

void MyUnit::rightClick(BWAPI::Unit target)
{
    if (issuedOrderThisFrame) return;
    if (!target || !target->exists()) return;

    // Unless is it a mineral field, don't click on the same target again
    if (!target->getType().isMineralField())
    {
        BWAPI::UnitCommand currentCommand(unit->getLastCommand());
        if (currentCommand.getType() == BWAPI::UnitCommandTypes::Right_Click_Unit &&
            currentCommand.getTargetPosition() == target->getPosition())
        {
            return;
        }
    }

    issuedOrderThisFrame = unit->rightClick(target);

#if DEBUG_UNIT_ORDERS
    CherryVis::log(unit) << "Order: Right-click " << target->getType() << " @ " << BWAPI::WalkPosition(target->getPosition());
#endif
}

void MyUnit::gather(BWAPI::Unit target)
{
    if (issuedOrderThisFrame) return;
    if (!target || !target->exists()) return;

    // Unless it is a mineral field, don't gather on the same target again
    if (!target->getType().isMineralField())
    {
        BWAPI::UnitCommand currentCommand(unit->getLastCommand());
        if (currentCommand.getType() == BWAPI::UnitCommandTypes::Gather &&
            currentCommand.getTargetPosition() == target->getPosition())
        {
            return;
        }
    }

    issuedOrderThisFrame = unit->gather(target);

#if DEBUG_UNIT_ORDERS
    CherryVis::log(unit) << "Order: Gather " << target->getType() << " @ " << BWAPI::WalkPosition(target->getPosition());
#endif
}

void MyUnit::returnCargo()
{
    if (issuedOrderThisFrame) return;

    issuedOrderThisFrame = unit->returnCargo();

#if DEBUG_UNIT_ORDERS
    CherryVis::log(unit) << "Order: Return cargo";
#endif
}

bool MyUnit::build(BWAPI::UnitType type, BWAPI::TilePosition tile)
{
    if (issuedOrderThisFrame) return false;
    if (!tile.isValid()) return false;

    issuedOrderThisFrame = unit->build(type, tile);

#if DEBUG_UNIT_ORDERS
    CherryVis::log(unit) << "Order: Build " << type << " @ " << BWAPI::WalkPosition(tile);
#endif

    return issuedOrderThisFrame;
}

void MyUnit::stop()
{
    if (issuedOrderThisFrame) return;

    issuedOrderThisFrame = unit->stop();

#if DEBUG_UNIT_ORDERS
    CherryVis::log(unit) << "Order: Stop";
#endif
}
