#pragma once

// This file is used to toggle between the different mining optimization "backends" that have existed through Stardust's history

// Simple backend that only ensures mineral locking
//#import "MineralLockingOptimization/MineralLockingOptimization.h"
//#define WORKERGATHEROPTIMIZER MineralLockingOptimization

// Backend that tries to optimize the order process timer only
//#import "OrderProcessTimerOptimization/WorkerOrderTimer.h"
//#define WORKERGATHEROPTIMIZER WorkerOrderTimer

// First implementation of the path-based optimizer
//#import "MiningOptimization/WorkerMiningOptimization.h"
//#define WORKERGATHEROPTIMIZER WorkerMiningOptimization

// Current implementation of the path-based optimizer
#import "MiningOptimizationV2/MiningOptimization.h"
#define WORKERGATHEROPTIMIZER MiningOptimization
