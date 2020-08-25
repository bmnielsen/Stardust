#pragma once

#include "Common.h"
#include "Play.h"
#include "Plays/MainArmy/MainArmyPlay.h"

class StrategyEngine
{
public:
    virtual ~StrategyEngine() = default;

    virtual void initialize(std::vector<std::shared_ptr<Play>> &plays) = 0;

    virtual void updatePlays(std::vector<std::shared_ptr<Play>> &plays) = 0;

    virtual void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                                  std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                  std::vector<std::pair<int, int>> &mineralReservations) = 0;

    virtual std::string getEnemyStrategy() { return "Unknown"; }

    virtual std::string getOurStrategy() { return "Unknown"; }

protected:
    static bool hasEnemyStolenOurGas();

    static void handleGasStealProduction(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                         int &zealotCount);

    static void upgradeAtCount(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                               BWAPI::UpgradeType upgradeType,
                               BWAPI::UnitType unitType,
                               int unitCount);

    static void upgradeWhenUnitStarted(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                       BWAPI::UpgradeType upgradeType,
                                       BWAPI::UnitType unitType,
                                       bool requireProducer = false);

    static void defaultGroundUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

    static void defaultExpansions(std::vector<std::shared_ptr<Play>> &plays);

    static void takeNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                     std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

    static void scoutExpos(std::vector<std::shared_ptr<Play>> &plays, int startingFrame);

    template<class T>
    static T *getPlay(std::vector<std::shared_ptr<Play>> &plays)
    {
        for (auto &play : plays)
        {
            if (auto match = std::dynamic_pointer_cast<T>(play))
            {
                return match.get();
            }
        }

        return nullptr;
    }

    static void UpdateDefendBasePlays(std::vector<std::shared_ptr<Play>> &plays);
};
