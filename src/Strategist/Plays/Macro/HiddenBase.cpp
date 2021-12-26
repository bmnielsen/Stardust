#include "HiddenBase.h"

#include "Map.h"
#include "Workers.h"
#include "Builder.h"

void HiddenBase::update()
{
    // Wait until we know where the hidden base is
    if (!base)
    {
        base = Map::getHiddenBase();
        if (!base) return;
    }

    // If our builder died, cancel the play
    // TODO: Is this too extreme?
    if (builder && !builder->exists())
    {
        status.complete = true;
        return;
    }

    // Stop the play when:
    // - We are past frame 15000
    // - We have taken our natural
    auto natural = Map::getMyNatural();
    if (currentFrame > 15000 ||
        (natural && natural->owner == BWAPI::Broodwar->self() && natural->resourceDepot && natural->resourceDepot->completed))
    {
        if (builder)
        {
            CherryVis::log(builder->id) << "Releasing from hidden base play";
            Builder::releaseReservedBuilder(builder);
            Workers::releaseWorker(builder);
        }

        status.complete = true;
        return;
    }

    // Reserve a builder and send it to the base when:
    // - Our main is saturated
    // - The path to the hidden base is safe
    if (!builder)
    {
        if (Workers::availableMineralAssignments(Map::getMyMain()) > 0) return;

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