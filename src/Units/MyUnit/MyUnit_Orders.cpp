#include "MyUnit.h"

void MyUnit::move(BWAPI::Position position)
{
    if (issuedOrderThisFrame) return;
    if (!position.isValid()) return;

    BWAPI::UnitCommand currentCommand(unit->getLastCommand());
    if (!unit->isStuck() &&
        currentCommand.getType() == BWAPI::UnitCommandTypes::Move &&
        currentCommand.getTargetPosition() == position &&
        unit->isMoving())
    {
        return;
    }

    issuedOrderThisFrame = unit->move(position);
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
}

void MyUnit::gather(BWAPI::Unit target)
{
    if (issuedOrderThisFrame) return;
    if (!target || !target->exists()) return;

    // Unless is it a mineral field, don't gather on the same target again
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
}

void MyUnit::returnCargo()
{
    if (issuedOrderThisFrame) return;

    issuedOrderThisFrame = unit->returnCargo();
}

bool MyUnit::build(BWAPI::UnitType type, BWAPI::TilePosition tile)
{
    if (issuedOrderThisFrame) return false;
    if (!tile.isValid()) return false;

    return issuedOrderThisFrame = unit->build(type, tile);
}

void MyUnit::stop()
{
    if (issuedOrderThisFrame) return;

    issuedOrderThisFrame = unit->stop();
}
