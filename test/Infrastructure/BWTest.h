#pragma once

#include <BWAPI.h>
#include "BWAPI/GameImpl.h"
#include "gtest/gtest.h"
#include "Maps.h"

struct UnitTypeAndPosition
{
public:
    BWAPI::UnitType type;
    bool waitForCreep;

    UnitTypeAndPosition(BWAPI::UnitType type, BWAPI::Position position, bool isCenter = false)
            : type(type), waitForCreep(false), position(position), tilePosition(BWAPI::TilePositions::Invalid), isCenter(isCenter) {}

    UnitTypeAndPosition(BWAPI::UnitType type, BWAPI::WalkPosition position, bool isCenter = false)
            : UnitTypeAndPosition(type, BWAPI::Position(position) + BWAPI::Position(4, 4), isCenter) {}

    UnitTypeAndPosition(BWAPI::UnitType type, BWAPI::TilePosition tilePosition, bool waitForCreep = false)
            : type(type), waitForCreep(waitForCreep), position(BWAPI::Positions::Invalid), tilePosition(tilePosition), isCenter(false) {}

    BWAPI::Position getCenterPosition() const;

private:
    BWAPI::Position position;
    BWAPI::TilePosition tilePosition;
    bool isCenter;
};

struct BWTest
{
public:
    std::shared_ptr<Maps::MapMetadata> map;
    std::vector<Maps::MapMetadata> maps;

    std::string opponentName = "Opponent";

    int frameLimit = 30000;
    int timeLimit = 600;
    int randomSeed = -1;
    bool expectWin = true;
    bool writeReplay = true;

    bool host = false;
    bool connect = false;

    char *sharedMemory = nullptr;

    std::string replayName;

    std::function<BWAPI::AIModule *()> myModule = nullptr;
    BWAPI::Race myRace = BWAPI::Races::Protoss;

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

    std::vector<BWAPI::TilePosition> removeStatic;

    void run();

    void addClockPositionToReplayName();

private:

    int initialUnitFrames = 0;
    std::unordered_map<int, std::vector<UnitTypeAndPosition>> myInitialUnitsByFrame;
    std::unordered_map<int, std::vector<UnitTypeAndPosition>> opponentInitialUnitsByFrame;

    void runGame(bool opponent);
};
