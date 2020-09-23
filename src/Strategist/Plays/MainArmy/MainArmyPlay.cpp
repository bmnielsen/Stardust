#include "MainArmyPlay.h"

#include "Units.h"

void MainArmyPlay::update()
{
    // Update detection - release observers when no longer needed, request observers when needed
    auto squad = getSquad();
    if (squad)
    {
        auto &detectors = squad->getDetectors();
        if (!squad->needsDetection() && !detectors.empty())
        {
            status.removedUnits.insert(status.removedUnits.end(), detectors.begin(), detectors.end());
        }
        else if (squad->needsDetection() && detectors.empty())
        {
            status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Observer, squad->getTargetPosition());
        }
    }
}

void MainArmyPlay::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Produce to fulfill any unit requirements
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.count < 1) continue;
        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 unitRequirement.type,
                                                                 unitRequirement.count,
                                                                 (unitRequirement.count + 1) / 2);
    }
}
