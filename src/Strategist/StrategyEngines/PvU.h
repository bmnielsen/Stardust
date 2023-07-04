#pragma once

#include "StrategyEngine.h"

class PvU : public StrategyEngine
{
public:
    // Against random we either open zealots or FFE
    enum class OurStrategy
    {
        TwoGateZealots,
        ForgeFastExpand,
    };
    static std::map<OurStrategy, std::string> OurStrategyNames;

    OurStrategy ourStrategy;

    PvU() : ourStrategy(OurStrategy::TwoGateZealots) {}

    void initialize(std::vector<std::shared_ptr<Play>> &plays, bool transitioningFromRandom) override;

    void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override;

    void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                          std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                          std::vector<std::pair<int, int>> &mineralReservations) override;
};
