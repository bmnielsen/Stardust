#pragma once

// This file contains common configuration parameters for the mining optimization, so they only have to be adjusted in one place

// Whether to use the next path lengths when scoring a path
// Currently disabled as it doesn't seem to have any net positive effect (some patches are improved, some are worsened)
#define USE_NEXT_PATH_LENGTHS false

// Defines how much we weight the estimated length of the next path in the scoring of a planned path
// We don't want to give it too much weight, since the data we have is averaged over many different situations, but on the other hand we want to
// make sure we nudge the worker onto a better cycle if it is at a local minima
// Results on testing with single worker on Vermeer:
// 0.0: 154.01
// 0.1: 154.04
// 0.2: 154.33
// 0.4: 154.17
// 0.5: 154.02
// 0.6: 154.34
#define NEXT_PATH_LENGTH_WEIGHT 0.0

// Defines the probability threshold we use to go for patch locking
#define PATCH_LOCK_THRESHOLD 0.99

// Defines which path cache to use
// #define PATH_CACHE_IMPLEMENTATION NullDeserializedPathCache
#define PATH_CACHE_IMPLEMENTATION DeserializedPathCache

// In the absence of pathing information, the distance from the patch where we assume a resend will always allow the worker to arrive on time
// TODO: Analyze the resend always arrives data to validate this value
#define ASSUME_RESEND_ALWAYS_ARRIVES_DISTANCE 20

// Whether to enable the gather takeover logic or just let the workers patch switch and resend
#define ENABLE_TAKEOVER_LOGIC true
