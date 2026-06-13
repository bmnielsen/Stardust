#pragma once

#include <set>

extern int currentFrame;

namespace OrderProcessTimer
{
    // Gets the number of frames to the previous order process timer reset, or 0 if a reset happens on the given frame
    int framesToPreviousReset(int frame);

    // Gets the number of frames to the previous order process timer reset, or 0 if a reset happens on this frame
    int framesToPreviousReset();

    // Gets the number of frames to the next order process timer reset, or 0 if a reset happens on the given frame
    int framesToNextReset(int frame);

    // Gets the number of frames to the next order process timer reset, or 0 if a reset happens on this frame
    int framesToNextReset();

    // Gets the next order process timer reset frame from the given frame. If the given frame is a reset frame, it is returned.
    int nextResetFrame(int frame);

    // Gets the next order process timer reset frame, or the current frame if a reset happens on this frame
    int nextResetFrame();

    // Gets the previous order process timer reset frame from the given frame. If the given frame is a reset frame, it is returned.
    int previousResetFrame(int frame);

    // Gets the previous order process timer reset frame, or the current frame if a reset happens on this frame
    int previousResetFrame();

    // Gets whether there is an order timer reset on the given frame
    bool isResetFrame(int frame);

    // Gets whether there is an order timer reset on this frame
    bool isResetFrame();

    // Computes the order process timer a unit will have at a frame delta compared to the given frame
    // Returns -1 if the order process timer cannot be predicted because of a reset
    int unitOrderProcessTimerAtDelta(int frame, int unitOrderProcessTimer, int frameDelta);

    // Computes the order process timer a unit will have at a frame delta compared to the current frame
    // Returns -1 if the order process timer cannot be predicted because of a reset
    int unitOrderProcessTimerAtDelta(int unitOrderProcessTimer, int frameDelta);

    // Takes a set of possible order process timer values at a start frame and returns the set of possible values after a delta
    // For this method, the values are considered at the start of a frame
    std::multiset<int> atStartOfFrameAtDelta(int startFrame,
                                             const std::multiset<int> &possibleStartingValues,
                                             const std::set<int> &gatherCommandFrames,
                                             const std::set<int> &returnCommandFrames,
                                             unsigned int frameDelta);

    // Advances the possible values from the end of this frame to the start of the next frame
    // Normally this just returns the same set, but if the next frame is a reset frame there are other possible values
    std::multiset<int> atStartOfNextFrame(int startFrame,
                                          const std::multiset<int> &atEndOfStartFrame);

}
