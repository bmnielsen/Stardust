#pragma once

// The maximum number of resends to explore during gather
#define GATHER_RESEND_LIMIT 2

// Defines the gather exploration horizon in number of frames to arrival
// We still record path data for positions outside the window, but only explore resends within it
#define GATHER_EXPLORATION_WINDOW_START 20
#define GATHER_EXPLORATION_WINDOW_END 3
