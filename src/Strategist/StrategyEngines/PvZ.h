#pragma once

#include "StrategyEngine.h"

class PvZ : public StrategyEngine
{
public:
    enum class ZergStrategy
    {
        Unknown,
        WorkerRush,             // Detected by seeing more than two workers in our main without other combat units
        SunkenContain,          // Hatch and sunkens at our natural
        ZerglingRush,           // Detected by seeing early pool or early lings
        PoolBeforeHatchery,     // e.g. 9-pool or overpool
        HatcheryBeforePool,     // e.g. 10 or 12 hatch
        ZerglingAllIn,          // Builds that get mass zerglings before other tech
        HydraBust,              // Builds that get mass hydras
        Turtle,                 // Mass sunkens
        Lair,                   // Have seen lair, indicates possibility of mutas or lurkers
        MutaRush,               // Builds that get mutas quickly
        // TODO: Mid- and late game
    };
    static std::map<ZergStrategy, std::string> ZergStrategyNames;

    enum class OurStrategy
    {
        EarlyGameDefense,       // We don't have scouting data yet
        AntiAllIn,              // For fast rushes or any serious early pressure, defends main until it can get tech out
        AntiSunkenContain,      // For when enemy builds sunkens at our natural
        SairSpeedlot,           // For when we do a FFE into sair/speedlot
        FastExpansion,          // For when we expand quickly
        Defensive,              // Cautious opening, for when we don't know if the opponent could be going for an all-in
        Normal,                 // Normal non-greedy and non-cautious opening
        MidGame,                // When we have reached the mid-game
        // TODO: Various mid-game and late-game strategies
    };
    static std::map<OurStrategy, std::string> OurStrategyNames;

    ZergStrategy enemyStrategy;
    OurStrategy ourStrategy;

    PvZ() : enemyStrategy(ZergStrategy::Unknown), ourStrategy(OurStrategy::EarlyGameDefense), enemyStrategyChanged(0) {}

    void initialize(std::vector<std::shared_ptr<Play>> &plays, bool transitioningFromRandom, const std::string &openingOverride) override;

    void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override;

    void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                          std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                          std::vector<std::pair<int, int>> &mineralReservations) override;

    std::string getEnemyStrategy() override { return ZergStrategyNames[enemyStrategy]; }

    std::string getOurStrategy() override { return OurStrategyNames[ourStrategy]; }

    bool isEnemyRushing() override
    {
        return enemyStrategy == ZergStrategy::WorkerRush ||
               enemyStrategy == ZergStrategy::ZerglingRush;
    }

private:
    int enemyStrategyChanged;

    ZergStrategy recognizeEnemyStrategy();

    OurStrategy chooseOurStrategy(ZergStrategy newEnemyStrategy, std::vector<std::shared_ptr<Play>> &plays);

    void handleNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays, std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

    static void handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

    void handleDetection(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

    bool isFFE();
};
