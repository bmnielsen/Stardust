#include "Strategist.h"

#include "Scout.h"
#include "Units.h"

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
        bool startedScouting = false;
        std::vector<ProductionGoal> productionGoals;
#ifndef _DEBUG
    }
#endif

    void update()
    {
        // For now always start scouting when our first pylon has been started
        if (!startedScouting && Units::countIncomplete(BWAPI::UnitTypes::Protoss_Pylon) > 0)
        {
            startedScouting = true;
            Scout::setScoutingMode(Scout::ScoutingMode::Location);
        }
    }

    void chooseOpening()
    {
        /* dt rush */
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dark_Templar, 2, 2));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Probe, -1, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon, 1));
        productionGoals.emplace_back(UpgradeProductionGoal(BWAPI::UpgradeTypes::Singularity_Charge));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon));
        //productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot));
        

        /* dragoons
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Probe, -1, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot, 1, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot, 2, 1));
        productionGoals.emplace_back(UpgradeProductionGoal(BWAPI::UpgradeTypes::Singularity_Charge));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon));
        */

        /* sair / zealot 
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Probe, -1, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot, 2, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Corsair, 1, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot, 4, 2));
        productionGoals.emplace_back(UpgradeProductionGoal(BWAPI::UpgradeTypes::Protoss_Ground_Weapons));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Corsair, 2, 1));
        productionGoals.emplace_back(UpgradeProductionGoal(BWAPI::UpgradeTypes::Protoss_Air_Weapons));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot, 6, 2));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Corsair, 8, 2));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot));
        */
    }

    std::vector<ProductionGoal> & currentProductionGoals()
    {
        return productionGoals;
    }
}
