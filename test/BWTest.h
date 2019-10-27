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
    int randomSeed = 42;
    bool expectWin = true;
    bool writeReplay = true;

    std::function<BWAPI::AIModule *()> myModule = nullptr;

    std::function<BWAPI::AIModule *()> opponentModule = nullptr;
    BWAPI::Race opponentRace = BWAPI::Races::Protoss;

    std::function<void()> onStartMine = nullptr;
    std::function<void()> onStartOpponent = nullptr;

    std::function<void()> onFrameMine = nullptr;
    std::function<void()> onFrameOpponent = nullptr;

    std::function<void()> onEndMine = nullptr;
    std::function<void()> onEndOpponent = nullptr;

    void run();

private:

    void runGame(bool opponent);
};
