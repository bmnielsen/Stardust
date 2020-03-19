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
    // Otherwise produce to fulfill our unit requirements
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.count < 1) continue;
        prioritizedProductionGoals[25].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                    unitRequirement.type,
                                                    unitRequirement.count,
                                                    (unitRequirement.count + 1) / 2);
    }

    // TODO
    int percentZealots = 0;

    int currentZealots = Units::countAll(BWAPI::UnitTypes::Protoss_Zealot);
    int currentDragoons = Units::countAll(BWAPI::UnitTypes::Protoss_Dragoon);

    int currentZealotRatio = (100 * currentZealots) / std::max(1, currentZealots + currentDragoons);

    if (currentZealotRatio >= percentZealots)
    {
        prioritizedProductionGoals[30].emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Dragoon, -1, -1);
    }
    prioritizedProductionGoals[30].emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Zealot, -1, -1);
}
