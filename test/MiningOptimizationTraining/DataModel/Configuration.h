#pragma once

// The maximum number of resends to explore
#define GATHER_RESEND_LIMIT 2
#define RETURN_RESEND_LIMIT 1

// Defines the gather exploration horizon in number of frames to arrival
// Nodes before the exploration window are still explored to see how resends affect the path, but longer paths are not explored further
#define GATHER_EXPLORATION_WINDOW_START 20
#define GATHER_EXPLORATION_WINDOW_END 5
#define RETURN_EXPLORATION_WINDOW_START INT_MAX
#define RETURN_EXPLORATION_WINDOW_END 5

// The number of exact positions to explore on each axis from each return path start position
// Should be a power of two such that it divides 256 evenly
#define EXACT_POSITIONS_TO_EXPLORE_PER_AXIS 4

// The number of start positions to explore from when initializing the path start positions to train on
// Should be a power of two such that it divides 256 evenly
#define START_POSITIONS_TO_EXPLORE_PER_AXIS 2
