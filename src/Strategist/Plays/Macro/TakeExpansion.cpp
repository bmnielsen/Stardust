#include "TakeExpansion.h"

#include "Builder.h"
#include "Workers.h"
#include "Units.h"
#include "Geo.h"
#include "PathFinding.h"

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

        Workers::reserveWorker(builder);
    }

    // For now, we just hand off control to the builder when we get close enough
    // TODO: Support escorting the worker, fortifying the expansion immediately

    auto depotCenter = BWAPI::Position(depotPosition) + BWAPI::Position(64, 48);

    int dist = Geo::EdgeToEdgeDistance(builder->getType(), builder->getPosition(), BWAPI::UnitTypes::Protoss_Nexus, depotCenter);
    if (dist < 64)
    {
        Builder::build(BWAPI::UnitTypes::Protoss_Nexus, depotPosition, builder);
        status.complete = true;
    }

    Units::getMine(builder).moveTo(depotCenter);
}

void TakeExpansion::addMineralReservations(std::vector<std::pair<int, int>> &mineralReservations)
{
    if (!builder) return;

    int travelTime =
            PathFinding::ExpectedTravelTime(builder->getPosition(),
                                            BWAPI::Position(depotPosition) + BWAPI::Position(64, 48),
                                            builder->getType(),
                                            PathFinding::PathFindingOptions::UseNearestBWEMArea);
    mineralReservations.emplace_back(std::make_pair(BWAPI::UnitTypes::Protoss_Nexus.mineralPrice(), travelTime));
}
