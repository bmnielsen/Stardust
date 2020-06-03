#pragma once

#include "StrategyEngine.h"

class PvT : public StrategyEngine
{
public:
    void initialize(std::vector<std::shared_ptr<Play>> &plays) override;

    void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override;

    void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                          std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                          std::vector<std::pair<int, int>> &mineralReservations) override;

    std::string getEnemyStrategy() override { return TerranStrategyNames[enemyStrategy]; }

    std::string getOurStrategy() override { return OurStrategyNames[ourStrategy]; }

private:
    enum class TerranStrategy
    {
        Unknown,
        GasSteal,           // Refinery in our main
        ProxyRush,          // Detected by seeing fewer buildings than expected in main
        MarineRush,         // Detected by seeing early gates or early zealots
        WallIn,             // Detected by scout blocked by buildings
        FastExpansion,      // Natural expansion taken early
        TwoFactory,         // The enemy has built two factories early
        Normal,             // Normal opening
        MidGame,            // Generic for when the opponent has transitioned out of their opening
        // TODO: Learn more Terran openings
        // TODO: Mid- and late game
    };
    static std::map<TerranStrategy, std::string> TerranStrategyNames;

    enum class OurStrategy
    {
        EarlyGameDefense,       // We don't have enough scouting data yet
        AntiMarineRush,         // For fast rushes, proxy rushes or any serious early pressure, defends main until it can get tech out
        FastExpansion,          // For when the opponent plays a greedy strategy
        Defensive,              // For when the opponent is playing an aggressive strategy that isn't considered a zealot rush
        Normal,                 // Normal non-greedy and non-cautious opening
        MidGame,                // When we have reached the mid-game
        // TODO: Various mid-game and late-game strategies
    };
    static std::map<OurStrategy, std::string> OurStrategyNames;

    TerranStrategy enemyStrategy;
    OurStrategy ourStrategy;
    int enemyStrategyChanged;

    TerranStrategy recognizeEnemyStrategy();

    OurStrategy chooseOurStrategy(TerranStrategy newEnemyStrategy, std::vector<std::shared_ptr<Play>> &plays);

    void handleNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays, std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

    static void handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

    void handleDetection(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);
};
