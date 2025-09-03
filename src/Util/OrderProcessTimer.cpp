#include "OrderProcessTimer.h"

#include <BWAPI.h>

#define FIRST_RESET_FRAME 8
#define RESET_FREQUENCY 150

namespace
{
    int botFrameToGameFrame(int botFrame)
    {
        // Convert based on the current difference between the engine and bot frames
        // Note that this might not be correct if a pause happens / happened between the current frame and the frame we are converting
        return botFrame + (BWAPI::Broodwar->getFrameCount() - currentFrame);
    }
}

namespace OrderProcessTimer
{
    int framesToPreviousReset(int frame)
    {
        return (botFrameToGameFrame(frame) - FIRST_RESET_FRAME) % RESET_FREQUENCY;
    }

    int framesToPreviousReset()
    {
        return framesToPreviousReset(currentFrame);
    }

    int framesToNextReset(int frame)
    {
        return (RESET_FREQUENCY - framesToPreviousReset(frame)) % RESET_FREQUENCY;
    }

    int framesToNextReset()
    {
        return framesToNextReset(currentFrame);
    }

    int nextResetFrame(int frame)
    {
        return frame + framesToNextReset(frame);
    }

    int nextResetFrame()
    {
        return nextResetFrame(currentFrame);
    }

    int previousResetFrame(int frame)
    {
        return frame - framesToPreviousReset(frame);
    }

    int previousResetFrame()
    {
        return previousResetFrame(currentFrame);
    }

    int isResetFrame(int frame)
    {
        return framesToPreviousReset(frame) == 0;
    }

    int isResetFrame()
    {
        return isResetFrame(currentFrame);
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
        return unitOrderProcessTimerAtDelta(currentFrame, unitOrderProcessTimer, frameDelta);
    }
}
