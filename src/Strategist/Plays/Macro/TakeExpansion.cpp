#include "TakeExpansion.h"

#include "Builder.h"
#include "Geo.h"

TakeExpansion::TakeExpansion(BWAPI::TilePosition depotPosition) : depotPosition(depotPosition), builder(nullptr) {}

void TakeExpansion::update()
{
    // If our builder is dead or renegaded, release it
    if (builder && (!builder->exists() || builder->getPlayer() != BWAPI::Broodwar->self()))
    {
        builder = nullptr;
    }

    // Reserve a builder if we don't have one
    if (!builder)
    {
        builder = Builder::getBuilderUnit(depotPosition, BWAPI::UnitTypes::Protoss_Nexus);
        if (!builder)
        {
            return;
        }

        Builder::build(BWAPI::UnitTypes::Protoss_Nexus, depotPosition, builder);

        // TODO: Support escorting the worker
    }

    // Treat the play as complete when the nexus has been created
    auto pendingBuilding = Builder::pendingHere(depotPosition);
    if (pendingBuilding && pendingBuilding->isConstructionStarted())
    {
        status.complete = true;

        // TODO: Transition to DefendBase play
    }
}

void TakeExpansion::cancel()
{
    Builder::cancel(depotPosition);
    status.complete = true;
}
