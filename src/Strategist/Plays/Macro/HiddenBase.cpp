#include "HiddenBase.h"

#include "Map.h"
#include "Workers.h"
#include "Builder.h"
#include "Units.h"
#include "Strategist.h"

void HiddenBase::update()
{
    // Wait until we know where the hidden base is
    if (!base)
    {
        base = Map::getHiddenBase();
        if (!base) return;
    }

    // Stop the play when:
    // - We are past frame 15000
    // - The builder died
    // - We have taken our natural
    // - The enemy has scouted the base
    if (currentFrame > 15000)
    {
        status.complete = true;
        return;
    }

    if (builder && !builder->exists())
    {
        status.complete = true;
        return;
    }

    auto natural = Map::getMyNatural();
    if (natural && natural->owner == BWAPI::Broodwar->self() && natural->resourceDepot && natural->resourceDepot->completed)
    {
        status.complete = true;
        return;
    }

    std::set<Unit> enemyUnits;
    Units::enemyInRadius(enemyUnits, base->getPosition(), 640);
    if (!enemyUnits.empty())
    {
        status.complete = true;
        return;
    }

    // Reserve a builder and send it to the base when:
    // - Our main is saturated
    // - Our main army is on the attack
    // - The path to the hidden base is safe
    if (!builder)
    {
        if (Workers::availableMineralAssignments(Map::getMyMain()) > 0) return;

        auto mainArmyPlay = Strategist::getMainArmyPlay();
        if (!mainArmyPlay) return;
        if (mainArmyPlay->isDefensive()) return;

        // TODO: Check path

        builder = Workers::getClosestReassignableWorker(base->getPosition(), false);
        if (!builder) return;

        CherryVis::log(builder->id) << "Reserving for hidden base play";

        Builder::addReservedBuilder(builder);
        Workers::reserveWorker(builder);
    }

    // Ensure the builder moves towards the hidden base when it isn't building anything
    if (!Builder::hasPendingBuilding(builder))
    {
        builder->moveTo(base->getPosition());
    }
}

void HiddenBase::disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                         const std::function<void(const MyUnit)> &movableUnitCallback)
{
    Builder::cancelBase(base);

    if (builder)
    {
        CherryVis::log(builder->id) << "Releasing from hidden base play";
        Builder::releaseReservedBuilder(builder);
        Workers::releaseWorker(builder);
    }

    Play::disband(removedUnitCallback, movableUnitCallback);
}
