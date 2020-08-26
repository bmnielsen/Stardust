#include "TakeExpansion.h"

#include "Builder.h"
#include "Geo.h"
#include "Units.h"
#include "Workers.h"
#include "Players.h"

TakeExpansion::TakeExpansion(Base *base)
        : Play((std::ostringstream() << "Take expansion @ " << base->getTilePosition()).str())
        , depotPosition(base->getTilePosition())
        , base(base)
        , builder(nullptr) {}

void TakeExpansion::update()
{
    // If our builder is dead, release it
    if (builder && !builder->exists())
    {
        builder = nullptr;
    }

    // Get a builder if we don't have one
    if (!builder)
    {
        builder = Builder::getBuilderUnit(depotPosition, BWAPI::UnitTypes::Protoss_Nexus);
        if (!builder)
        {
            return;
        }

        // TODO: Support escorting the worker
    }

    // If the base is blocked by an enemy unit, build a cannon to clear it
    if (base->blockedByEnemy)
    {
        // If there are no static defense locations available, cancel the play
        auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
        if (baseStaticDefenseLocations.first == BWAPI::TilePositions::Invalid ||
            baseStaticDefenseLocations.second.empty())
        {
            status.complete = true;
            return;
        }

        // Ensure the builder stays reserved between buildings
        Workers::reserveWorker(builder);

        // First build the pylon
        auto pylon = Units::myBuildingAt(baseStaticDefenseLocations.first);
        if (!pylon)
        {
            if (!Builder::pendingHere(baseStaticDefenseLocations.first))
            {
                Builder::build(BWAPI::UnitTypes::Protoss_Pylon, baseStaticDefenseLocations.first, builder);
            }

            return;
        }

        // Next build the cannon
        if (pylon->completed)
        {
            auto cannon = Units::myBuildingAt(*baseStaticDefenseLocations.second.begin());
            if (!cannon)
            {
                if (!Builder::pendingHere(*baseStaticDefenseLocations.second.begin()))
                {
                    Builder::build(BWAPI::UnitTypes::Protoss_Photon_Cannon, *baseStaticDefenseLocations.second.begin(), builder);
                }

                return;
            }
        }

        // Falling through to here means the builder doesn't have any work to do

        // Try to find the blocking unit
        Unit blocker = nullptr;
        for (auto unit : Units::allEnemy())
        {
            if (unit->isFlying) continue;
            if (!unit->lastPositionValid) continue;
            if (Geo::Overlaps(depotPosition, 4, 3, unit->getTilePosition(), 2, 2))
            {
                blocker = unit;
                break;
            }
        }

        // If we found the blocking unit, attack it
        if (blocker)
        {
            std::vector<std::pair<MyUnit, Unit>> dummyUnitsAndTargets;
            builder->attackUnit(blocker, dummyUnitsAndTargets);
            return;
        }

        // We didn't find the blocking unit
        // If we have detection on all of the depot's tiles, clear the base being blocked by enemy
        auto detectionGrid = Players::grid(BWAPI::Broodwar->self());
        bool allDetected = true;
        for (int x = depotPosition.x; x < depotPosition.x + 4; x++)
        {
            for (int y = depotPosition.y; y < depotPosition.y + 3; y++)
            {
                BWAPI::TilePosition here(x, y);
                if (detectionGrid.detection(BWAPI::Position(here) + BWAPI::Position(16, 16)) == 0)
                {
                    allDetected = false;
                    goto exitOuterLoop;
                }
            }
        }
        exitOuterLoop:;

        if (!allDetected)
        {
            return;
        }

        Log::Get() << "Base @ " << base->getTilePosition() << " is no longer blocked by an enemy unit";
        base->blockedByEnemy = false;
    }

    if (!Builder::isPendingHere(depotPosition))
    {
        Builder::build(BWAPI::UnitTypes::Protoss_Nexus, depotPosition, builder);
    }

    // Treat the play as complete when the nexus is finished
    auto nexus = Units::myBuildingAt(depotPosition);
    if (nexus && nexus->completed)
    {
        status.complete = true;
    }

    // Also cancel the play if the base becomes owned by someone else in the meantime
    if (base->owner)
    {
        status.complete = true;
    }
}

bool TakeExpansion::constructionStarted() const
{
    return Units::myBuildingAt(depotPosition) != nullptr;
}

void TakeExpansion::disband(const std::function<void(const MyUnit &)> &removedUnitCallback,
                            const std::function<void(const MyUnit &)> &movableUnitCallback)
{
    Builder::cancel(depotPosition);

    if (builder) Workers::releaseWorker(builder);
}
