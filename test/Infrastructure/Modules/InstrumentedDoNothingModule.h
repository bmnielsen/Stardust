#pragma once

#include "DoNothingModule.h"

#include "Log.h"
#include "CherryVis.h"

class InstrumentedDoNothingModule : public DoNothingModule
{
public:
    void onStart() override
    {
        currentFrame = 0;

        Log::initialize();
        Log::SetDebug(true);
        Log::SetOutputToConsole(true);
        CherryVis::initialize();
    }

    void onFrameStart()
    {
        currentFrame++;
    }

    void onFrameEnd()
    {
        CherryVis::frameEnd(currentFrame);
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
};
