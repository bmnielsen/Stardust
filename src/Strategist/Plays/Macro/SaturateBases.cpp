#include "SaturateBases.h"

#include "Units.h"
#include "Workers.h"

void SaturateBases::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Hard cap of 75 workers
    if (Units::countAll(BWAPI::UnitTypes::Protoss_Probe) >= 75) return;

    // Produce workers until there are no available resources at owned bases
    // TODO: At some point we need to consider which depots are safe to produce workers from
    int countToProduce = Workers::availableMineralAssignments() - Units::countIncomplete(BWAPI::UnitTypes::Protoss_Probe);
    if (countToProduce > 0)
    {
        prioritizedProductionGoals[10].emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Probe, countToProduce, 1);
    }
}