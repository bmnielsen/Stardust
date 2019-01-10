#include "Strategist.h"

#include "Opening.h"
#include "Zealots.h"
#include "Dragoons.h"
#include "DarkTemplar.h"

namespace Strategist
{
#ifndef _DEBUG
    namespace
    {
#endif
        std::vector<ProductionGoal> productionGoals;
#ifndef _DEBUG
    }
#endif

    void chooseOpening()
    {
        // TODO: Implement proper openings
        //productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Probe, 17, 1));
        //productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon, 1, 1));

        /* dt rush
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dark_Templar, 2, 2));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Probe, -1, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon, 1));
        productionGoals.emplace_back(UpgradeProductionGoal(BWAPI::UpgradeTypes::Singularity_Charge));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot));
        */

        /* dragoons */
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Probe, -1, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot, 2));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon, 1));
        productionGoals.emplace_back(UpgradeProductionGoal(BWAPI::UpgradeTypes::Singularity_Charge));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon));
    }

    std::vector<ProductionGoal> & currentProductionGoals()
    {
        return productionGoals;
    }
}
