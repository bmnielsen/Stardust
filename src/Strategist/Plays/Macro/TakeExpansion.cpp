#include "TakeExpansion.h"

#include "Builder.h"
#include "Geo.h"

TakeExpansion::TakeExpansion(BWAPI::TilePosition depotPosition) : depotPosition(depotPosition), builder(nullptr) {}

void TakeExpansion::update()
{
    // If our builder is dead, release it
    if (builder && !builder->exists())
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

    // TODO: Create DefendBase play when nexus is started? Or transition to it when nexus finishes?

    // Treat the play as complete when the nexus is finished
    auto pendingBuilding = Builder::pendingHere(depotPosition);
    if (!pendingBuilding)
    {
        status.complete = true;
    }
}

bool TakeExpansion::constructionStarted() const
{
    if (!builder) return false;

    auto pendingBuilding = Builder::pendingHere(depotPosition);
    if (!pendingBuilding) return true; // completed

    return pendingBuilding->isConstructionStarted();
}

void TakeExpansion::disband(const std::function<void(const MyUnit &)> &removedUnitCallback,
                            const std::function<void(const MyUnit &)> &movableUnitCallback)
{
    Builder::cancel(depotPosition);
}
