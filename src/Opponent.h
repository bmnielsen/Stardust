#pragma once

#include "Common.h"

namespace Opponent
{
    void initialize();

    void update();

    void gameEnd(bool isWinner);

    std::string &getName();

    bool isUnknownRace();

    bool hasRaceJustBeenDetermined();

    void setGameValue(const std::string &key, int value);

    void incrementGameValue(const std::string &key, int delta = 1);

    bool isGameValueSet(const std::string &key);

    void addMyStrategyChange(const std::string &strategy);

    void addEnemyStrategyChange(const std::string &strategy);

    int minValueInPreviousGames(const std::string &key, int defaultNoData, int maxCount = INT_MAX, int minCount = 0);

    double winLossRatio(double defaultValue, int maxCount = INT_MAX);

    // Picks the next opening from the given set of openings using UCB1 based on win rate of the given openings
    std::string selectOpeningUCB1(const std::vector<std::string> &openings, double decayFactor = 0.05);
}
