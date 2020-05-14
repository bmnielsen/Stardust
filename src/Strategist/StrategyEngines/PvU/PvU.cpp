#include "StrategyEngines/PvU.h"

#include "Plays/Macro/SaturateBases.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Plays/Scouting/EarlyGameWorkerScout.h"

void PvU::initialize(std::vector<std::shared_ptr<Play>> &plays)
{
    plays.emplace_back(std::make_shared<SaturateBases>());
    plays.emplace_back(std::make_shared<EarlyGameWorkerScout>());
    plays.emplace_back(std::make_shared<DefendMyMain>());
}

void PvU::updatePlays(std::vector<std::shared_ptr<Play>> &plays)
{
    // This engine never updates plays - it transitions to another engine once the enemy race is known
}

void PvU::updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                           std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                           std::vector<std::pair<int, int>> &mineralReservations)
{
    // Always start with two-gate zealots until we know what race the opponent is
    prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                               BWAPI::UnitTypes::Protoss_Zealot,
                                                               -1,
                                                               2);
}
