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
