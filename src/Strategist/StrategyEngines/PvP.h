#pragma once

#include "StrategyEngine.h"

class PvP : public StrategyEngine
{
public:
    PvP() : enemyStrategy(ProtossStrategy::Unknown), ourStrategy(OurStrategy::EarlyGameDefense), enemyStrategyChanged(0) {}

    void initialize(std::vector<std::shared_ptr<Play>> &plays) override;

    void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override;

    void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                          std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                          std::vector<std::pair<int, int>> &mineralReservations) override;

    std::string getEnemyStrategy() override { return ProtossStrategyNames[enemyStrategy]; }

    std::string getOurStrategy() override { return OurStrategyNames[ourStrategy]; }

private:
    enum class ProtossStrategy
    {
        Unknown,
        GasSteal,           // Assimilator in our main
        ProxyRush,          // Detected by seeing fewer buildings than expected in main
        ZealotRush,         // Detected by seeing early gates or early zealots
        EarlyForge,         // Detected by seeing a forge before core or second gate, can indicate forge expand, "fake" forge expand, or cannon rush
        TwoGate,            // Two or more gateways before core
        OneGateCore,        // One gateway before core
        FastExpansion,      // Natural expansion taken early
        BlockScouting,      // The enemy has blocked our scout from getting into their main, suspect shenanigans
        ZealotAllIn,        // Builds that get a lot of zealots before other tech
        DragoonAllIn,       // Builds that get a lot of dragoons before other tech
        Turtle,             // Early cannons or FFE
        DarkTemplarRush,    // Dark templar or dark templar tech
        MidGame,            // Generic for when the opponent has transitioned out of their opening
        // TODO: Mid- and late game
    };
    static std::map<ProtossStrategy, std::string> ProtossStrategyNames;

    enum class OurStrategy
    {
        EarlyGameDefense,       // We don't have enough scouting data yet
        AntiZealotRush,         // For fast rushes, proxy rushes or any serious early pressure, defends main until it can get tech out
        FastExpansion,          // For when the opponent plays a greedy or turtle strategy
        Defensive,              // For when the opponent is playing an aggressive strategy that isn't considered a zealot rush
        Normal,                 // Normal non-greedy and non-cautious opening
        DTExpand,               // Reaction to dragoon all-in, gets one DT, moves out and expands behind it
        MidGame,                // When we have reached the mid-game
        // TODO: Various mid-game and late-game strategies
    };
    static std::map<OurStrategy, std::string> OurStrategyNames;

    ProtossStrategy enemyStrategy;
    OurStrategy ourStrategy;
    int enemyStrategyChanged;

    ProtossStrategy recognizeEnemyStrategy();

    OurStrategy chooseOurStrategy(ProtossStrategy newEnemyStrategy, std::vector<std::shared_ptr<Play>> &plays);

    void handleNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays, std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

    static void handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

    void handleDetection(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);
};
