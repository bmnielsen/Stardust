#pragma once

extern int frameNow;

namespace OrderProcessTimer
{
    /* Gets the number of frames to the previous order process timer reset, or 0 if a reset happens on the given frame */
    int framesToPreviousReset(int tick);

    /* Gets the number of frames to the previous order process timer reset, or 0 if a reset happens on this frame */
    int framesToPreviousReset();

    /* Gets the number of frames to the next order process timer reset, or 0 if a reset happens on the given frame */
    int framesToNextReset(int tick);

    /* Gets the number of frames to the next order process timer reset, or 0 if a reset happens on this frame */
    int framesToNextReset();

    /* Gets the next order process timer reset frame, or 0 if a reset happens on the given frame */
    int nextResetFrame(int tick);

    /* Gets the next order process timer reset frame, or 0 if a reset happens on this frame */
    int nextResetFrame();

    /* Gets the previous order process timer reset frame, or 0 if a reset happens on the given frame */
    int previousResetFrame(int tick);

    /* Gets the previous order process timer reset frame, or 0 if a reset happens on this frame */
    int previousResetFrame();

    /* Gets whether there is an order timer reset on the given frame */
    int isResetFrame(int tick);

    /* Gets whether there is an order timer reset on this frame */
    int isResetFrame();

    /* Computes the order process timer a unit will have at a frame delta compared to the given frame */
    /* Returns -1 if the order process timer cannot be predicted because of a reset */
    int unitOrderProcessTimerAtDelta(int tick, int unitOrderProcessTimer, int frameDelta);

    /* Computes the order process timer a unit will have at a frame delta compared to the current frame */
    /* Returns -1 if the order process timer cannot be predicted because of a reset */
    int unitOrderProcessTimerAtDelta(int unitOrderProcessTimer, int frameDelta);
}
