#pragma once

#include <BWAPI.h>
#include "BWAPI/GameImpl.h"
#include "gtest/gtest.h"

struct BWTest
{
public:
    std::string map = "maps/sscai/(4)Fighting Spirit.scx";

    int frameLimit = 25000;
    int timeLimit = 600;

    BWAPI::AIModule *opponentModule = nullptr;
    BWAPI::Race opponentRace = BWAPI::Races::Protoss;

    std::function<void(BWAPI::BroodwarImpl_handle)> onStartMine = nullptr;
    std::function<void(BWAPI::BroodwarImpl_handle)> onStartOpponent = nullptr;

    std::function<void(BWAPI::BroodwarImpl_handle)> onFrameMine = nullptr;
    std::function<void(BWAPI::BroodwarImpl_handle)> onFrameOpponent = nullptr;

    std::function<void(BWAPI::BroodwarImpl_handle)> onEndMine = nullptr;
    std::function<void(BWAPI::BroodwarImpl_handle)> onEndOpponent = nullptr;

    void run();

private:

    void runGame(bool opponent);
};
