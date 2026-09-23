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

    // The time limit for a combat sim execution in microseconds
    // There will always be a high limit returned to avoid extremely slow performance
    // In frame limited configurations the limit will be lower, and will be dynamically adjusted lower again if the frame has already
    // been taking a long time to process
    unsigned int combatSimTimeLimitMicroseconds();

    // Whether we should skip running the producer this frame
    // The caller may ignore this if the producer hasn't been run for a while
    bool shouldSkipProducer();

    // Whether we should avoid running the mining optimization solver for gather paths
    // Return paths are always allowed since they are much simpler
    bool shouldAvoidMiningOptimizationGatherSolver();
}
