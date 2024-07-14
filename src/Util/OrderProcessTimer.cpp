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
}