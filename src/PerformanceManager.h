#pragma once

namespace PerformanceManager
{
    // Initialize to reset objects for a new game
    void initialize();

    // Signal the start of a frame, so the performance manager can start tracking the frame performance
    void frameStart();

    // Signal the end of a frame, so the performance manager can keep track of overall performance statistics
    // Returns true if the frame was over the defined limit
    bool frameEnd();

    // Signal the end of a game, so the performance manager can write any output it wants to keep
    void gameEnd();

    // Signal that we have finished our updates (refreshing unit states, building placements, etc.)
    // This is essentially the baseline frame time that we always allow the bot to use regardless of performance
    void frameFinishedUpdateStep();


}
