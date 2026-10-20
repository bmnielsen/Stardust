#pragma once

#include "StrategyEngine.h"

class PvT : public StrategyEngine
{
public:
    enum class TerranStrategy
    {
        Unknown,
        WorkerRush,         // Detected by seeing more than two workers in our main without other combat units
        ProxyRush,          // Detected by seeing fewer buildings than expected in main
        MarineRush,         // Detected by seeing early gates or early zealots
        WallIn,             // Detected by scout blocked by buildings
        BlockScouting,      // The enemy has blocked our scout from getting into their main, suspect shenanigans
        FastExpansion,      // Natural expansion taken early
        TwoFactory,         // The enemy has built two factories early
        NormalOpening,      // Normal opening
        MidGameMech,        // Opponent has transitioned from opening into a mech-heavy composition
        MidGameBio,         // Opponent has transitioned from opening into a bio-heavy composition
        /* TODO: Learn more Terran openings */
        /* TODO: Mid- and late game */
    };
    static std::map<TerranStrategy, std::string> TerranStrategyNames;

    enum class OurStrategy
    {
        ForgeExpandGoons,       // FFE strategy we use against Random
        EarlyGameDefense,       // We don't have enough scouting data yet
        AntiMarineRush,         // For fast rushes, proxy rushes or any serious early pressure, defends main until it can get tech out
        FastExpansion,          // For when the opponent plays a greedy strategy
        Defensive,              // For when the opponent is playing an aggressive strategy that isn't considered a zealot rush
        NormalOpening,          // Normal non-greedy and non-cautious opening
        MidGame,                // When we have reached the mid-game
        /* TODO: More mid- and late-game strategies */
    };
    static std::map<OurStrategy, std::string> OurStrategyNames;

    TerranStrategy enemyStrategy;
    OurStrategy ourStrategy;

    PvT() : enemyStrategy(TerranStrategy::Unknown), ourStrategy(OurStrategy::EarlyGameDefense), enemyStrategyChanged(0) {}

    void initialize(std::vector<std::shared_ptr<PlayItem>> &playList, bool transitioningFromRandom, const std::string &openingOverride) override;

    void updatePlays(std::vector<std::shared_ptr<PlayItem>> &playList) override;

    void updateProduction(std::vector<std::shared_ptr<PlayItem>> &playList,
                          std::map<int, std::vector<ProductionGoal>> &prioProdGoals,
                          std::vector<std::pair<int, int>> &mineralReservations) override;

    std::string getEnemyStrategy() override { return TerranStrategyNames[enemyStrategy]; }

    std::string getOurStrategy() override { return OurStrategyNames[ourStrategy]; }

    bool isEnemyRushing() override
    {
        return enemyStrategy == TerranStrategy::WorkerRush ||
               enemyStrategy == TerranStrategy::ProxyRush ||
               enemyStrategy == TerranStrategy::MarineRush;
    }

    bool isEnemyProxy() override
    {
        return enemyStrategy == TerranStrategy::ProxyRush;
    }

private:
    int enemyStrategyChanged;

    TerranStrategy recognizeEnemyStrategy();

    /* P2-D: declaration-table recognition (same predicates/thresholds as the */
    /* switch path, expressed as a RecognizerShared transition table). Runs in */
    /* shadow mode until zero-divergence is proven, then replaces the switch. */
    TerranStrategy recognizeEnemyStrategyTable(TerranStrategy current);

    OurStrategy chooseOurStrategy(TerranStrategy newEnemyStrategy, std::vector<std::shared_ptr<PlayItem>> &playList);

    void handleNaturalExpansion(std::vector<std::shared_ptr<PlayItem>> &playList, std::map<int, std::vector<ProductionGoal>> &prioProdGoals);

    void handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioProdGoals);

    void handleDetection(std::vector<std::shared_ptr<PlayItem>> &playList, std::map<int, std::vector<ProductionGoal>> &prioProdGoals);
};
