#pragma once

#include "StrategyEngine.h"

class PvP : public StrategyEngine
{
public:
    enum class ProtossStrategy
    {
        Unknown,
        WorkerRush,         // Detected by seeing more than two workers in our main without other combat units
        ProxyRush,          // Detected by seeing fewer buildings than expected in main
        ZealotRush,         // Detected by seeing early gates or early zealots
        EarlyForge,         // Detected by seeing a forge before core or second gate, can indicate forge expand, "fake" forge expand, or cannon rush
        TwoGate,            // Two or more gateways before core
        NoZealotCore,       // One gateway before core
        OneZealotCore,      // One gateway and one (or more) zealots before core
        FastExpansion,      // Natural expansion taken early
        BlockScouting,      // The enemy has blocked our scout from getting into their main, suspect shenanigans
        ZealotAllIn,        // Builds that get a lot of zealots before other tech
        DragoonAllIn,       // Builds that get a lot of dragoons before other tech
        EarlyRobo,          // Builds that get a robotics bay fairly early
        Turtle,             // Early cannons or FFE
        DarkTemplarRush,    // Dark templar or dark templar tech
        MidGame,            // Generic for when the opponent has transitioned out of their opening
        /* TODO: Mid- and late game */
    };
    static std::map<ProtossStrategy, std::string> ProtossStrategyNames;

    enum class OurStrategy
    {
        ForgeExpandDT,          // FFE strategy we use against Random and as a counter to 4-gate / other player DT expand
        TwoGateDT,              // Build that starts with 2-gate zealots and transitions into DT
        EarlyGameDefense,       // We don't have enough scouting data yet
        AntiZealotRush,         // For fast rushes, proxy rushes or any serious early pressure, defends main until it can get tech out
        AntiDarkTemplarRush,    // For responding to a DT rush
        FastExpansion,          // For when the opponent plays a greedy or turtle strategy
        Defensive,              // For when the opponent is playing an aggressive strategy that isn't considered a zealot rush
        Normal,                 // Normal non-greedy and non-cautious opening
        OneGateCoreNoZealot,    // S7: core-first, no zealots, dragoons + range (the nzcore line PW/BB standard on; production via oneGateCoreOpening(0))
        ThreeGateRobo,          // 3 gate robo opening
        DTExpand,               // Reaction to dragoon all-in, gets one DT, moves out and expands behind it
        MidGame,                // When we have reached the mid-game
        /* TODO: Various mid-game and late-game strategies */
    };
    static std::map<OurStrategy, std::string> OurStrategyNames;

    ProtossStrategy enemyStrategy;
    OurStrategy ourStrategy;

    PvP() : enemyStrategy(ProtossStrategy::Unknown), ourStrategy(OurStrategy::EarlyGameDefense), enemyStrategyChanged(0) {}

    void initialize(std::vector<std::shared_ptr<PlayItem>> &playList, bool transitioningFromRandom, const std::string &openingOverride) override;

    void updatePlays(std::vector<std::shared_ptr<PlayItem>> &playList) override;

    void updateProduction(std::vector<std::shared_ptr<PlayItem>> &playList,
                          std::map<int, std::vector<ProductionGoal>> &prioProdGoals,
                          std::vector<std::pair<int, int>> &mineralReservations) override;

    std::string getEnemyStrategy() override { return ProtossStrategyNames[enemyStrategy]; }

    std::string getOurStrategy() override { return OurStrategyNames[ourStrategy]; }

    bool isEnemyRushing() override
    {
        return enemyStrategy == ProtossStrategy::WorkerRush ||
               enemyStrategy == ProtossStrategy::ProxyRush ||
               enemyStrategy == ProtossStrategy::ZealotRush;
    }

    bool isEnemyProxy() override
    {
        return enemyStrategy == ProtossStrategy::ProxyRush;
    }

private:
    int enemyStrategyChanged;

    /* P1-A/P1-B (2026-09-20 report v2): late-game transition + one-base exit */
    bool allinExitDue();
    void handleLateTransition(std::map<int, std::vector<ProductionGoal>> &prioProdGoals);
    int allinExitCheckedAt = -1;
    bool allinExitDueCache = false;

    ProtossStrategy recognizeEnemyStrategy();

    /* P2-E: declaration-table recognition (same predicates/thresholds as the */
    /* switch path, expressed as a RecognizerShared transition table). Runs in */
    /* shadow mode until zero-divergence is proven, then replaces the switch. */
    ProtossStrategy recognizeEnemyStrategyTable(ProtossStrategy current);

    OurStrategy chooseOurStrategy(ProtossStrategy newEnemyStrategy, std::vector<std::shared_ptr<PlayItem>> &playList);

    void handleNaturalExpansion(std::vector<std::shared_ptr<PlayItem>> &playList, std::map<int, std::vector<ProductionGoal>> &prioProdGoals);

    static void handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioProdGoals);

    void handleDetection(std::map<int, std::vector<ProductionGoal>> &prioProdGoals);
};
