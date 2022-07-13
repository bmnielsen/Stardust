#include "MainArmyPlay.h"

#include "Units.h"

void MainArmyPlay::update()
{
    // Update detection - release observers when no longer needed, request observers when needed
    auto squad = getSquad();
    if (squad && squad->needsDetection())
    {
        auto &detectors = squad->getDetectors();
        if (detectors.size() < 2)
        {
            status.unitRequirements.emplace_back(2 - detectors.size(), BWAPI::UnitTypes::Protoss_Observer, squad->getTargetPosition());
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
                                                                 label,
                                                                 unitRequirement.type,
                                                                 unitRequirement.count,
                                                                 (unitRequirement.count + 1) / 2);
    }
}
