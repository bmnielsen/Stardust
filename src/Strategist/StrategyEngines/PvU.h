#pragma once

#include "StrategyEngine.h"

class PvU : public StrategyEngine
{
public:
    /* Against random we either open zealots or FFE */
    enum class OurStrategy
    {
        TwoGateZealots,
        ForgeFastExpand,
    };
    static std::map<OurStrategy, std::string> OurStrategyNames;

    OurStrategy ourStrategy;

    PvU() : ourStrategy(OurStrategy::TwoGateZealots) {}

    void initialize(std::vector<std::shared_ptr<PlayItem>> &playList, bool transitioningFromRandom, const std::string &openingOverride) override;

    void updatePlays(std::vector<std::shared_ptr<PlayItem>> &playList) override;

    void updateProduction(std::vector<std::shared_ptr<PlayItem>> &playList,
                          std::map<int, std::vector<ProductionGoal>> &prioProdGoals,
                          std::vector<std::pair<int, int>> &mineralReservations) override;
};
