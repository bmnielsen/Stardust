#include "MainArmyPlay.h"

#include "Strategies.h"
#include "Units.h"

namespace
{
    int computeDesiredDetectorCount(const auto &squad)
    {
        int result = 0;
        if (Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Dark_Templar) ||
            Strategist::isOurStrategy(PvP::OurStrategy::AntiDarkTemplarRush))
        {
            result = 1;
        }
        if (squad->needsDetection())
        {
            result = 2;
        }

        return result;
    }
}

void MainArmyPlay::update()
{
    // Request detection when needed
    auto squad = getSquad();
    if (squad)
    {
        auto desiredDetectors = computeDesiredDetectorCount(squad);
        auto &detectors = squad->getDetectors();
        if (detectors.size() < desiredDetectors)
        {
            status.unitRequirements.emplace_back(desiredDetectors - detectors.size(), BWAPI::UnitTypes::Protoss_Observer, squad->getTargetPosition());
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

bool MainArmyPlay::isExcessDetector(const MyUnit &unit) const
{
    if (unit->type != BWAPI::UnitTypes::Protoss_Observer) return false;
    auto detector = std::dynamic_pointer_cast<MyObserverImpl>(unit);
    if (!detector) return false;

    // This indicates the play has no units
    if (detector->getActivity() == ObserverActivity::None) return true;

    auto squad = getSquad();
    if (!squad) return false;

    // Don't allow it if we need the number of detectors we have
    auto desiredDetectors = computeDesiredDetectorCount(squad);
    auto &detectors = squad->getDetectors();
    if (detectors.size() <= desiredDetectors) return false;

    // Prioritize letting them go in this order:
    // - EscortingArmy
    // - ScoutingEnemyArmy
    // - DetectingEnemy
    if (detector->getActivity() == ObserverActivity::EscortingArmy) return true;

    for (const auto &squadDetector : detectors)
    {
        if (squadDetector->getActivity() == ObserverActivity::EscortingArmy) return false;
        if (squadDetector->getActivity() == ObserverActivity::ScoutingEnemyArmy)
        {
            return detector->getActivity() == ObserverActivity::ScoutingEnemyArmy;
        }
    }

    // All of them are detecting an enemy, so just allow any of them to get taken
    return true;
}
