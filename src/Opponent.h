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

    int minValueInPreviousGames(const std::string &key, int defaultTooFewResults, int defaultNoData, int maxCount = INT_MAX, int minCount = 0);

}
