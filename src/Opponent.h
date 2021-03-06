#pragma once

#include "Common.h"

namespace Opponent
{
    void initialize();

    std::string &getName();

    bool isUnknownRace();

    bool hasRaceJustBeenDetermined();
}
