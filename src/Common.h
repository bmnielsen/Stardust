#pragma once

#include <BWAPI.h>
#include <set>
#include "Log.h"
#include "CherryVis.h"

// Defines that control instrumentation across the whole project
// Instrumentation of specific features is enabled in the relevant files
// The overall CHERRYVIS_ENABLED define is in CherryVis.h

#if INSTRUMENTATION_ENABLED
#define DEBUG_COMBATSIM false    // Writes log messages for each sim result
#endif

#if INSTRUMENTATION_ENABLED_VERBOSE
#define DEBUG_UNIT_ORDERS true   // Writes a log message for each order sent to a unit
#define DEBUG_GRID_UPDATES false // Writes a log message whenever a grid is updated
#endif
