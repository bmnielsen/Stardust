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
        // Some more explanation for this:
        // The normal BWAPI flow calls the bot prior to running the game engine on a given frame. This is somewhat confusing, since if we look at
        // our bot's log output on a frame and compare it with the game data at the end of that frame in e.g. CherryVis, they are misaligned.
        // As a result of this, we subtract one from the bot's currentFrame at the start of the game, such that from the bot's perspective, it gets
        // called at the end of the frame rather than at the start.
        // This makes things like order process timer resets a bit more tricky to handle. The calculation here makes it so that the methods in
        // this file treat an order process timer reset as happening on the bot frame on which the reset happened. So for example, the order process
        // timer reset on game frame 158 will be reported as happening on bot frame 158, even though the game frame will have advanced again by the
        // time bot frame 158 is being processed.
        return botFrame + (BWAPI::Broodwar->getFrameCount() - currentFrame) - 1;
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

    std::multiset<int> atStartOfFrameAtDelta(int startFrame, // NOLINT(*-no-recursion)
                                             const std::multiset<int> &possibleStartingValues,
                                             const std::set<int> &gatherCommandFrames,
                                             const std::set<int> &returnCommandFrames,
                                             unsigned int frameDelta)
    {
        if (frameDelta == 0) return possibleStartingValues;

        int endFrame = startFrame + (int)frameDelta;

        // Find the last command frame that takes effect within the window
        int lastCommandFrameTakesEffect = -1;
        bool lastCommandIsGather = false;
        auto processCommandFrames = [&](const std::set<int> &commandFrames, bool isGather)
        {
            for (auto commandFrame : commandFrames)
            {
                int frameResendTakesEffect = commandFrame + BWAPI::Broodwar->getLatencyFrames() + 1; // Plus one as we are aligned to start of frame
                if (frameResendTakesEffect > lastCommandFrameTakesEffect && frameResendTakesEffect <= endFrame)
                {
                    lastCommandFrameTakesEffect = frameResendTakesEffect;
                    lastCommandIsGather = isGather;
                }
            }
        };
        processCommandFrames(gatherCommandFrames, true);
        processCommandFrames(returnCommandFrames, false);

        // If there is a resend that takes effect, reset the order process timer values accordingly
        // On gather the order process timer goes to 0 for two frames while the command is processed
        // On return the order process timer goes to 0 for one frame while the command is processed
        if (lastCommandFrameTakesEffect > startFrame
            || (lastCommandFrameTakesEffect == startFrame && lastCommandIsGather))
        {
            if (lastCommandFrameTakesEffect == endFrame)
            {
                return {0};
            }
            if (isResetFrame(lastCommandFrameTakesEffect + 1))
            {
                return atStartOfFrameAtDelta(
                        lastCommandFrameTakesEffect + 1,
                        {0, 1, 2, 3, 4, 5, 6, 7},
                        {},
                        {},
                        frameDelta - (lastCommandFrameTakesEffect + 1 - startFrame));
            }
            return atStartOfFrameAtDelta(
                    lastCommandFrameTakesEffect + 1,
                    {lastCommandIsGather ? 0 : 8},
                    {},
                    {},
                    frameDelta - (lastCommandFrameTakesEffect + 1 - startFrame));
        }

        // If there is a reset frame within the window, the values will reset to 0-7 inclusive at the start of that frame
        // We don't include the start frame since a reset there has already been taken into account in the initial options
        int framesToNextReset = OrderProcessTimer::framesToNextReset(startFrame + 1) + 1;
        if (framesToNextReset <= frameDelta)
        {
            // Recursively call again after the reset
            return atStartOfFrameAtDelta(
                    startFrame + framesToNextReset,
                    {0, 1, 2, 3, 4, 5, 6, 7},
                    {},
                    {},
                    frameDelta - framesToNextReset);
        }

        // Nothing has happened that would interfere with the normal cycle, so run it on all the values
        std::multiset<int> result;
        for (auto startingValue : possibleStartingValues)
        {
            startingValue -= (int)frameDelta;
            while (startingValue < 0) startingValue += 9;
            result.insert(startingValue);
        }
        return result;
    }
}
