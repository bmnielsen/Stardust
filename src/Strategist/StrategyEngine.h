#pragma once

#include "Common.h"
#include "Play.h"
#include "Plays/MainArmy/MainArmyPlay.h"

class StrategyEngine
{
public:
    virtual ~StrategyEngine() = default;

    virtual void initialize(std::vector<std::shared_ptr<Play>> &plays, bool transitioningFromRandom) = 0;

    virtual void updatePlays(std::vector<std::shared_ptr<Play>> &plays) = 0;

    virtual void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                                  std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                  std::vector<std::pair<int, int>> &mineralReservations) = 0;

    virtual std::string getEnemyStrategy() { return "Unknown"; }

    virtual std::string getOurStrategy() { return "Unknown"; }

    virtual bool isEnemyRushing() { return false; }

    virtual bool isEnemyProxy() { return false; }

    template<class T>
    static T *getPlay(std::vector<std::shared_ptr<Play>> &plays)
    {
        for (auto &play: plays)
        {
            if (auto match = std::dynamic_pointer_cast<T>(play))
            {
                return match.get();
            }
        }

        return nullptr;
    }

    template<class T>
    static auto beforePlayIt(std::vector<std::shared_ptr<Play>> &plays)
    {
        auto it = plays.begin();
        for (; it != plays.end(); it++)
        {
            if (std::dynamic_pointer_cast<T>(*it) != nullptr)
            {
                break;
            }
        }
        return it;
    };

protected:
    static bool hasEnemyStolenOurGas();

    static void handleGasStealProduction(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                         int &zealotCount);

    static void handleAntiRushProduction(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                         int dragoonCount,
                                         int zealotCount,
                                         int zealotsRequired,
                                         int zealotProducerLimit = 2);

    static bool handleIslandExpansionProduction(std::vector<std::shared_ptr<Play>> &plays,
                                                std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

    static void cancelTrainingUnits(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                    BWAPI::UnitType type,
                                    int requiredCapacity = INT_MAX,
                                    int remainingTrainingTimeThreshold = 0);

    static void oneGateCoreOpening(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                   int dragoonCount,
                                   int zealotCount,
                                   int desiredZealots);

    static void mainArmyProduction(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                   BWAPI::UnitType unitType,
                                   int count,
                                   int &highPriorityCount,
                                   int producerLimit = -1);

    static void upgradeAtCount(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                               UpgradeOrTechType upgradeOrTechType,
                               BWAPI::UnitType unitType,
                               int unitCount,
                               int level = 1);

    static void upgradeWhenUnitCreated(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                       UpgradeOrTechType upgradeOrTechType,
                                       BWAPI::UnitType unitType,
                                       bool requireCompletedUnit = false,
                                       bool requireProducer = false,
                                       int priority = PRIORITY_NORMAL);

    static void defaultGroundUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

    static void upgrade(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                        UpgradeOrTechType upgradeOrTechType,
                        int level = 1,
                        int priority = PRIORITY_NORMAL);

    static void defaultExpansions(std::vector<std::shared_ptr<Play>> &plays);

    static void takeNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                     std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

    static void cancelNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                       std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

    static void takeExpansionWithShuttle(std::vector<std::shared_ptr<Play>> &plays);

    static void scoutExpos(std::vector<std::shared_ptr<Play>> &plays, int startingFrame);

    static void updateDefendBasePlays(std::vector<std::shared_ptr<Play>> &plays);

    static void updateAttackPlays(std::vector<std::shared_ptr<Play>> &plays, bool defendOurMain);

    static void updateSpecialTeamsPlays(std::vector<std::shared_ptr<Play>> &plays);

    static void reserveMineralsForExpansion(std::vector<std::pair<int, int>> &mineralReservations);

    static void buildDefensiveCannons(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                      bool atChoke = true,
                                      int frameNeeded = 0,
                                      int atBases = 0);

    static bool hasCannonAtWall();
};
