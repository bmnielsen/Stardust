#include "HiddenBase.h"

#include "Map.h"
#include "Workers.h"
#include "Builder.h"
#include "Units.h"
#include "Strategist.h"

void HiddenBase::update()
{
    /* Wait until we know where the hidden base is */
    if (!baseLoc)
    {
        baseLoc = Map::getHiddenBase();
        if (!baseLoc) return;
    }

    /* Stop the play when: */
    /* - We are past frame 15000 */
    /* - The builder died */
    /* - We have taken our natural */
    /* - The enemy has scouted the base */
    if (frameNow > 15000)
    {
        status.complete = true;
        return;
    }

    if (builder && !builder->exists())
    {
        status.complete = true;
        return;
    }

    auto natBase = Map::getMyNatural();
    if (natBase && natBase->owner == BWAPI::Broodwar->self() && natBase->resourceDepot && natBase->resourceDepot->completed)
    {
        status.complete = true;
        return;
    }

    std::set<Unit> enemyUnits;
    UnitMgr::enemyInRadius(enemyUnits, baseLoc->getPosition(), 640);
    if (!enemyUnits.empty())
    {
        status.complete = true;
        return;
    }

    /* Reserve a builder and send it to the base when: */
    /* - Our main is saturated */
    /* - Our main army is on the attack */
    /* - The path to the hidden base is safe */
    if (!builder)
    {
        if (WorkerMgr::availableMineralAssignments(Map::getMyMain()) > 0) return;

        auto mainArmyPlay = Director::getMainArmyPlay();
        if (!mainArmyPlay) return;
        if (mainArmyPlay->isDefensive()) return;

        /* TODO: Check path */

        builder = WorkerMgr::getClosestReassignableWorker(baseLoc->getPosition(), false);
        if (!builder) return;

        Telemetry::log(builder->ident) << "Reserving for hidden base play";

        Constructor::addReservedBuilder(builder);
        WorkerMgr::reserveWorker(builder);
    }

    /* Ensure the builder moves towards the hidden base when it isn't building anything */
    if (!Constructor::hasPendingBuilding(builder))
    {
        builder->moveTo(baseLoc->getPosition());
    }
}

void HiddenBase::disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                         const std::function<void(const OwnedUnit)> &movableUnitCallback)
{
    Constructor::cancelBase(baseLoc);

    if (builder)
    {
        Telemetry::log(builder->ident) << "Releasing from hidden base play";
        Constructor::releaseReservedBuilder(builder);
        WorkerMgr::releaseWorker(builder);
    }

    PlayItem::disband(removedUnitCallback, movableUnitCallback);
}
