#include "OrderProcessTimer.h"

#include <BWAPI.h>

#define FIRST_RESET_FRAME 8
#define RESET_FREQUENCY 150

namespace OrderProcessTimer
{
    int framesToPreviousReset(int frame)
    {
        return (frame - FIRST_RESET_FRAME) % RESET_FREQUENCY;
    }

    int framesToPreviousReset()
    {
        return framesToPreviousReset(BWAPI::Broodwar->getFrameCount());
    }

    int framesToNextReset(int frame)
    {
        return (RESET_FREQUENCY - framesToPreviousReset(frame)) % RESET_FREQUENCY;
    }

    int framesToNextReset()
    {
        return framesToNextReset(BWAPI::Broodwar->getFrameCount());
    }

    int nextResetFrame(int frame)
    {
        return frame + framesToNextReset(frame);
    }

    int nextResetFrame()
    {
        return nextResetFrame(BWAPI::Broodwar->getFrameCount());
    }

    int previousResetFrame(int frame)
    {
        return frame - framesToPreviousReset(frame);
    }

    int previousResetFrame()
    {
        return previousResetFrame(BWAPI::Broodwar->getFrameCount());
    }

    int isResetFrame(int frame)
    {
        return framesToPreviousReset(frame) == 0;
    }

    int isResetFrame()
    {
        return isResetFrame(BWAPI::Broodwar->getFrameCount());
    }

    int unitOrderProcessTimerAtDelta(int frame, int unitOrderProcessTimer, int frameDelta)
    {
        if (frameDelta == 0) return unitOrderProcessTimer;
        if (unitOrderProcessTimer == -1) return -1;

        if (frameDelta > 0)
        {
            if (framesToNextReset(frame + 1) < frameDelta) return -1;

            int result = unitOrderProcessTimer - frameDelta;
            while (result < 0) result += 9;
            return result;
        }

        if (framesToPreviousReset(frame) < (-frameDelta)) return -1;

        int result = unitOrderProcessTimer - frameDelta;
        while (result > 8) result -= 9;
        return result;
    }

    int unitOrderProcessTimerAtDelta(int unitOrderProcessTimer, int frameDelta)
    {
        return unitOrderProcessTimerAtDelta(BWAPI::Broodwar->getFrameCount(), unitOrderProcessTimer, frameDelta);
    }
}
