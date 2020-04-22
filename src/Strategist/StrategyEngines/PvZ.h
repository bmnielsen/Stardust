#pragma once

#include "StrategyEngine.h"

class PvZ : public StrategyEngine
{
public:
    PvZ() : enemyStrategy(ZergStrategy::Unknown) {}

    void initialize(std::vector<std::shared_ptr<Play>> &plays) override;

    void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override;

    void updateProduction(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                          std::vector<std::pair<int, int>> &mineralReservations) override;

private:
    enum class ZergStrategy
    {
        Unknown,
        ZerglingRush,           // Detected by seeing early pool or early lings
        PoolBeforeHatchery,     // e.g. 9-pool or overpool
        HatcheryBeforePool,     // e.g. 10 or 12 hatch
        ZerglingAllIn,          // Builds that get mass zerglings before other tech
        Turtle,                 // Mass sunkens
        Lair,                   // Have seen lair but don't know what mid-game strategy will be played
        MidGameMutalisks,       // Expect mid-game mutalisks, detected by seeing spire
        MidGameLurkers,         // Expect mid-game lurkers, detected by seeing lurker or lurker egg
    };

    ZergStrategy enemyStrategy;

    void updateStrategyRecognition();

    void handleMainArmy(std::vector<std::shared_ptr<Play>> &plays);

    static void handleNaturalExpansion(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

    static void handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

    static void handleDetection(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);
};
