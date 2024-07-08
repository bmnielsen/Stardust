#pragma once

#include "DoNothingModule.h"

#include "Log.h"
#include "CherryVis.h"

#include "Map.h"
#include "Units.h"
#include "Players.h"

class InstrumentedDoNothingModule : public DoNothingModule
{
private:
    bool includeInfoModules;

public:
    explicit InstrumentedDoNothingModule(bool includeInfoModules = false) : includeInfoModules(includeInfoModules) {}

    void onStart() override
    {
        currentFrame = 0;

        Log::initialize();
        Log::SetDebug(true);
        Log::SetOutputToConsole(true);
        CherryVis::initialize();

        if (includeInfoModules)
        {
            Units::initialize();
            Map::initialize();
            Players::initialize();
        }
    }

    void onFrameStart()
    {
        if (includeInfoModules)
        {
            Units::update();
            Players::update();
            Map::update();
        }
    }

    void onFrameEnd()
    {
        CherryVis::frameEnd(currentFrame++);
    }

    void onFrame() override
    {
        onFrameStart();
        onFrameEnd();
    }

    void onEnd(bool isWinner) override
    {
        CherryVis::gameEnd();
    }

    void onUnitCreate(BWAPI::Unit unit) override
    {
        CherryVis::unitFirstSeen(unit);
    }

    void onUnitDiscover(BWAPI::Unit unit) override
    {
        if (includeInfoModules)
        {
            Map::onUnitDiscover(unit);
        }
    }

    void onUnitDestroy(BWAPI::Unit unit) override
    {
        if (includeInfoModules)
        {
            Map::onUnitDestroy(unit);
            Units::onUnitDestroy(unit);
        }
    }
};
