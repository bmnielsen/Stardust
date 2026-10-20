#include "StrategyEngines/PvU.h"

#include "Plays/Macro/SaturateBases.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Plays/MainArmy/ForgeFastExpand.h"
#include "Plays/Scouting/EarlyGameWorkerScout.h"
#include "Opponent.h"
#include "StrategySelector.h"

std::map<PvU::OurStrategy, std::string> PvU::OurStrategyNames = {
        {OurStrategy::TwoGateZealots,  "TwoGateZealots"},
        {OurStrategy::ForgeFastExpand, "ForgeFastExpand"}
};

void PvU::initialize(std::vector<std::shared_ptr<PlayItem>> &playList, bool transitioningFromRandom, const std::string &openingOverride)
{
    if (!openingOverride.empty()) playList.clear();

    playList.emplace_back(std::make_shared<SaturateBases>());
    playList.emplace_back(std::make_shared<EarlyGameWorkerScout>());

    std::string opening = openingOverride;
    if (opening.empty())
    {
        /* L2 seam (S4): JSON strategy library first (legality + Tier-0); "" */
        /* falls through to the legacy selection below. */
        /* PvU design constraint: this runs while the enemy race is still unknown, */
        /* so a matchup="PvU" strategy must stay race-agnostic; once the race is */
        /* revealed Strategist re-initializes the concrete matchup engine, which */
        /* runs its own seam - an illegal-for-race choice self-corrects there. */
        opening = StrategySelector::selectOpening("PvU");
    }
    if (opening.empty()) opening = StrategySelector::selectOpeningScored("PvU");
    if (opening == OurStrategyNames[OurStrategy::ForgeFastExpand] && BuildingPlacement::hasForgeGatewayWall())
    {
        playList.emplace_back(std::make_shared<ForgeFastExpand>());
        ourStrategy = OurStrategy::ForgeFastExpand;
    }
    else
    {
        playList.emplace_back(std::make_shared<DefendMyMain>());
    }
    AppLog::Get() << "Selected opening " << OurStrategyNames[ourStrategy];

    Opponent::addMyStrategyChange(OurStrategyNames[ourStrategy]);
    Opponent::addEnemyStrategyChange("RandomUnknown");
}

void PvU::updatePlays(std::vector<std::shared_ptr<PlayItem>> &playList)
{
    /* This engine never updates plays - it transitions to another engine once the enemy race is known */
}

void PvU::updateProduction(std::vector<std::shared_ptr<PlayItem>> &playList,
                           std::map<int, std::vector<ProductionGoal>> &prioProdGoals,
                           std::vector<std::pair<int, int>> &mineralReservations)
{
    /* Always start with two-gate zealots until we know what race the opponent is */
    prioProdGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                               "SE",
                                                               BWAPI::UnitTypes::Protoss_Zealot,
                                                               -1,
                                                               2);
}
