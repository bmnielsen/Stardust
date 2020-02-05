#include <Players/Players.h>
#include "MyUnit.h"

#include "Players.h"
#include "UnitUtil.h"
#include "Geo.h"

MyUnit::MyUnit(BWAPI::Unit unit)
        : Unit(unit)
        , issuedOrderThisFrame(false)
        , targetPosition(BWAPI::Positions::Invalid)
        , currentlyMovingTowards(BWAPI::Positions::Invalid)
        , grid(nullptr)
        , gridNode(nullptr)
        , lastMoveFrame(0)
{
}

void MyUnit::update(BWAPI::Unit unit)
{
    Unit::update(unit);

    if (!unit || !unit->exists()) return;

    issuedOrderThisFrame = false;

    typeSpecificUpdate();

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

bool MyUnit::unstick()
{
    if (unit->isStuck())
    {
        stop();
        return true;
    }

    return false;
}
