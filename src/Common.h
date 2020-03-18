#pragma once

#include <BWAPI.h>
#include <set>
#include "Log.h"
#include "CherryVis.h"

// Defines that control various levels of instrumentation
// The overall CHERRYVIS_ENABLED define is in CherryVis.h

#define DEBUG_GRID_UPDATES false // Writes a log message whenever a grid is updated
#define DEBUG_COMBATSIM false    // Writes log messages for each sim result
#define DEBUG_COMBATSIM_CSV false  // Writes a CSV file for each cluster with detailed sim information
#define DEBUG_UNIT_ORDERS true   // Writes a log message for each order sent to a unit

// These defines configure a per-frame summary of various unit type's orders, commands, etc.
#define DEBUG_PROBE_STATUS false
#define DEBUG_ZEALOT_STATUS true
#define DEBUG_DRAGOON_STATUS true
#define DEBUG_SHUTTLE_STATUS false

// Heatmaps are quite large, so we don't always want to write them every frame
// These defines configure what frequency to dump them, or 0 to disable them
#define COLLISION_HEATMAP_FREQUENCY 0
#define GROUND_THREAT_HEATMAP_FREQUENCY 0
#define AIR_THREAT_HEATMAP_FREQUENCY 0
#define DETECTION_HEATMAP_FREQUENCY 0
#define VISIBILITY_HEATMAP_FREQUENCY 0
