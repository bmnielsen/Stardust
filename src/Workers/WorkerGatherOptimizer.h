#pragma once

// This file is used to toggle between the different mining optimization "backends" that have existed through Stardust's history

// The current mineral and gas forecasting constants are also stored here, since our mineral income rate changes between the engines.
// In reality these aren't as simple as a single constant, since patches with a single worker produce slightly more efficiently per worker
// than patches with two workers, there is a difference between near and far patches, and we get a boost at full saturation from patch
// locking.
// Generally a worker assigned alone to a patch will return minerals approximately every 150 frames, a worker assigned with another worker
// to a patch will return minerals approximately every 173 frames, and workers at a fully saturated base will return minerals approximately
// every 166 frames, but efficiency is reduced when cannons are in the mineral line.
// This is heavily map and base dependent, however, since it varies depending on how BW does its pathing.

// Simple backend that only ensures mineral locking
// #include "MineralLockingOptimization/MineralLockingOptimization.h"
// #define WORKERGATHEROPTIMIZER MineralLockingOptimization
// #define MINERALS_PER_WORKER_FRAME 0.0465

// Backend that tries to optimize the order process timer only
// #include "OrderProcessTimerOptimization/WorkerOrderTimer.h"
// #define WORKERGATHEROPTIMIZER WorkerOrderTimer
// #define MINERALS_PER_WORKER_FRAME 0.0465
// #define GAS_PER_WORKER_FRAME 0.071
// #define MINERALS_PER_GAS_UNIT 0.655

// First implementation of the path-based optimizer
// #include "MiningOptimization/WorkerMiningOptimization.h"
// #define WORKERGATHEROPTIMIZER WorkerMiningOptimization
// #define MINERALS_PER_WORKER_FRAME 0.0472
// #define GAS_PER_WORKER_FRAME 0.071
// #define MINERALS_PER_GAS_UNIT 0.665

// Current implementation of the path-based optimizer
#include "MiningOptimizationV2/MiningOptimization.h"
#define WORKERGATHEROPTIMIZER MiningOptimization
#define MINERALS_PER_WORKER_FRAME 0.0488
#define GAS_PER_WORKER_FRAME 0.071
#define MINERALS_PER_GAS_UNIT 0.687
