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

    if (cooldown <= BWAPI::Broodwar->getRemainingLatencyFrames())
    {
        // Attack if we expect to be in range after latency frames
        auto predictedPosition = UnitUtil::PredictPosition(unit, BWAPI::Broodwar->getRemainingLatencyFrames());
        auto predictedTargetPosition = UnitUtil::PredictPosition(target, BWAPI::Broodwar->getRemainingLatencyFrames());
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