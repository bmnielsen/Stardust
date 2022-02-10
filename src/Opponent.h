#pragma once

#include "Common.h"

namespace Opponent
{
    void initialize();

    void gameEnd(bool isWinner);

    std::string &getName();

    bool isUnknownRace();

    bool hasRaceJustBeenDetermined();

    void setGameValue(const std::string &key, int value);

    bool isGameValueSet(const std::string &key);

    int minValueInPreviousGames(const std::string &key, int defaultNoData, int maxCount = INT_MAX, int minCount = 0);

    double winLossRatio(double defaultValue, int maxCount = INT_MAX);
}
