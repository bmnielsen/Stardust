#include "StrategyEngines/PvU.h"

#include "Plays/Macro/SaturateBases.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Plays/MainArmy/ForgeFastExpand.h"
#include "Plays/Scouting/EarlyGameWorkerScout.h"
#include "Opponent.h"

std::map<PvU::OurStrategy, std::string> PvU::OurStrategyNames = {
        {OurStrategy::TwoGateZealots,  "TwoGateZealots"},
        {OurStrategy::ForgeFastExpand, "ForgeFastExpand"}
};

void PvU::initialize(std::vector<std::shared_ptr<Play>> &plays, bool transitioningFromRandom, const std::string &openingOverride)
{
    if (!openingOverride.empty()) plays.clear();

    plays.emplace_back(std::make_shared<SaturateBases>());
    plays.emplace_back(std::make_shared<EarlyGameWorkerScout>());

    std::string opening = openingOverride;
    if (opening.empty())
    {
        opening = Opponent::selectOpeningUCB1(
                {
                        OurStrategyNames[OurStrategy::ForgeFastExpand],
                        OurStrategyNames[OurStrategy::TwoGateZealots]
                });
    }
    if (opening == OurStrategyNames[OurStrategy::ForgeFastExpand] && BuildingPlacement::hasForgeGatewayWall())
    {
        plays.emplace_back(std::make_shared<ForgeFastExpand>());
        ourStrategy = OurStrategy::ForgeFastExpand;
    }
    else
    {
        plays.emplace_back(std::make_shared<DefendMyMain>());
    }
    Log::Get() << "Selected opening " << OurStrategyNames[ourStrategy];

    Opponent::addMyStrategyChange(OurStrategyNames[ourStrategy]);
    Opponent::addEnemyStrategyChange("RandomUnknown");
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
                                                               "SE",
                                                               BWAPI::UnitTypes::Protoss_Zealot,
                                                               -1,
                                                               2);
}
