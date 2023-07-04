#pragma once

#include "StrategyEngine.h"

class PlasmaStrategyEngine : public StrategyEngine
{
public:
    PlasmaStrategyEngine() : enemyStrategy(EnemyStrategy::Unknown) {}

    void initialize(std::vector<std::shared_ptr<Play>> &plays, bool transitioningFromRandom) override;

    void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override;

    void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                          std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                          std::vector<std::pair<int, int>> &mineralReservations) override;

    std::string getEnemyStrategy() override { return EnemyStrategyNames[enemyStrategy]; }

    std::string getOurStrategy() override { return "Plasma"; }

private:
    enum class EnemyStrategy
    {
        Unknown,
        ProxyRush,          // Detected by seeing fewer buildings than expected in main
        Normal,             // Final state when we are satisfied the enemy is not doing a rush
    };
    static std::map<EnemyStrategy, std::string> EnemyStrategyNames;
    EnemyStrategy enemyStrategy;

    EnemyStrategy recognizeEnemyStrategy();

    static void handleNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                       std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);
};
