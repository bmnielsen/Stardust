#include <Players/Players.h>
#include "MyUnit.h"

#include "Players.h"
#include "UnitUtil.h"
#include "Geo.h"

MyUnit::MyUnit(BWAPI::Unit unit)
    : unit(unit)
    , issuedOrderThisFrame(false)
    , targetPosition(BWAPI::Positions::Invalid)
    , currentlyMovingTowards(BWAPI::Positions::Invalid)
    , lastMoveFrame(0)
{
}

void MyUnit::update()
{
    if (!unit || !unit->exists()) { return; }

    issuedOrderThisFrame = false;

    typeSpecificUpdate();

    updateMoveWaypoints();
}


void MyUnit::attackUnit(BWAPI::Unit target)
{
    int cooldown = target->isFlying() ? unit->getAirWeaponCooldown() : unit->getGroundWeaponCooldown();
    int range = Players::weaponRange(BWAPI::Broodwar->self(), target->isFlying() ? unit->getType().airWeapon() : unit->getType().groundWeapon());

    // If the target is already in range, just attack it
    if (unit->getDistance(target) <= range)
    {
        attack(target);
        return;
    }

    // Otherwise attack when our cooldown is almost over and we expect to be in range shortly
    if (cooldown <= BWAPI::Broodwar->getRemainingLatencyFrames() + 2)
    {
        auto predictedPosition = UnitUtil::PredictPosition(unit, BWAPI::Broodwar->getRemainingLatencyFrames() + 2);
        auto predictedTargetPosition = UnitUtil::PredictPosition(target, BWAPI::Broodwar->getRemainingLatencyFrames() + 2);
        auto predictedDist = Geo::EdgeToEdgeDistance(unit->getType(), predictedPosition, target->getType(), predictedTargetPosition);

        if (predictedDist <= range)
        {
            attack(target);
            return;
        }
    }

    // Plot an intercept course
    auto intercept = Geo::FindInterceptPoint(unit, target);
    if (!intercept.isValid()) intercept = UnitUtil::PredictPosition(target, 5);
    move(intercept);
}