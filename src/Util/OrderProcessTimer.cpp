#include "OrderProcessTimer.h"

#include <BWAPI.h>

#define FIRST_RESET_FRAME 8
#define RESET_FREQUENCY 150

namespace
{
    int botFrameToGameFrame(int botFrame)
    {
        /* Convert based on the current difference between the engine and bot frames */
        /* Note that this might not be correct if a pause happens / happened between the current frame and the frame we are converting */
        return botFrame + (BWAPI::Broodwar->getFrameCount() - frameNow);
    }
}

namespace OrderProcessTimer
{
    int framesToPreviousReset(int tick)
    {
        return (botFrameToGameFrame(tick) - FIRST_RESET_FRAME) % RESET_FREQUENCY;
    }

    int framesToPreviousReset()
    {
        return framesToPreviousReset(frameNow);
    }

    int framesToNextReset(int tick)
    {
        return (RESET_FREQUENCY - framesToPreviousReset(tick)) % RESET_FREQUENCY;
    }

    int framesToNextReset()
    {
        return framesToNextReset(frameNow);
    }

    int nextResetFrame(int tick)
    {
        return tick + framesToNextReset(tick);
    }

    int nextResetFrame()
    {
        return nextResetFrame(frameNow);
    }

    int previousResetFrame(int tick)
    {
        return tick - framesToPreviousReset(tick);
    }

    int previousResetFrame()
    {
        return previousResetFrame(frameNow);
    }

    int isResetFrame(int tick)
    {
        return framesToPreviousReset(tick) == 0;
    }

    int isResetFrame()
    {
        return isResetFrame(frameNow);
    }

    int unitOrderProcessTimerAtDelta(int tick, int unitOrderProcessTimer, int frameDelta)
    {
        if (frameDelta == 0) return unitOrderProcessTimer;
        if (unitOrderProcessTimer == -1) return -1;

        if (frameDelta > 0)
        {
            if (framesToNextReset(tick + 1) < frameDelta) return -1;

            int outcome = unitOrderProcessTimer - frameDelta;
            while (outcome < 0) outcome += 9;
            return outcome;
        }

        if (framesToPreviousReset(tick) < (-frameDelta)) return -1;

        int outcome = unitOrderProcessTimer - frameDelta;
        while (outcome > 8) outcome -= 9;
        return outcome;
    }

    int unitOrderProcessTimerAtDelta(int unitOrderProcessTimer, int frameDelta)
    {
        return unitOrderProcessTimerAtDelta(frameNow, unitOrderProcessTimer, frameDelta);
    }
}
