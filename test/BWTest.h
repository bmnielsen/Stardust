#pragma once

#include <BWAPI.h>
#include "BWAPI/GameImpl.h"
#include "gtest/gtest.h"

struct UnitTypeAndPosition
{
public:
    BWAPI::UnitType type;
    bool waitForCreep;

    UnitTypeAndPosition(BWAPI::UnitType type, BWAPI::Position position)
            : type(type), waitForCreep(false), position(position), tilePosition(BWAPI::TilePositions::Invalid) {}

    UnitTypeAndPosition(BWAPI::UnitType type, BWAPI::WalkPosition position)
            : UnitTypeAndPosition(type, BWAPI::Position(position) + BWAPI::Position(2, 2)) {}

    UnitTypeAndPosition(BWAPI::UnitType type, BWAPI::TilePosition tilePosition, bool waitForCreep = false)
            : type(type), waitForCreep(waitForCreep), position(BWAPI::Positions::Invalid), tilePosition(tilePosition) {}

    BWAPI::Position getCenterPosition();

private:
    BWAPI::Position position;
    BWAPI::TilePosition tilePosition;

};

struct BWTest
{
public:
    std::string map = "maps/sscai/(4)Fighting Spirit.scx";

    int frameLimit = 25000;
    int timeLimit = 600;
    int randomSeed = 42;
    bool expectWin = true;
    bool writeReplay = true;

    char *sharedMemory = nullptr;

    std::string replayName;

    std::function<BWAPI::AIModule *()> myModule = nullptr;

    std::function<BWAPI::AIModule *()> opponentModule = nullptr;
    BWAPI::Race opponentRace = BWAPI::Races::Protoss;

    std::function<void()> onStartMine = nullptr;
    std::function<void()> onStartOpponent = nullptr;

    std::function<void()> onFrameMine = nullptr;
    std::function<void()> onFrameOpponent = nullptr;

    std::function<void(bool)> onEndMine = nullptr;
    std::function<void(bool)> onEndOpponent = nullptr;

    std::vector<UnitTypeAndPosition> myInitialUnits;
    std::vector<UnitTypeAndPosition> opponentInitialUnits;

    void run();

private:

    int initialUnitFrames = 0;
    std::unordered_map<int, std::vector<UnitTypeAndPosition>> myInitialUnitsByFrame;
    std::unordered_map<int, std::vector<UnitTypeAndPosition>> opponentInitialUnitsByFrame;

    void runGame(bool opponent);
};
