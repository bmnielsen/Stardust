#pragma once

#if INSTRUMENTATION_ENABLED
#define DEBUG_COMBATSIM false       // Draws each sim result and writes log messages for interesting transitions
#define DEBUG_COMBATSIM_LOG false   // Writes log messages for each sim result
#endif

#if INSTRUMENTATION_ENABLED_VERBOSE
#define DEBUG_COMBATSIM_CVIS true        // Writes combat sim data to cherryvis
#define DEBUG_COMBATSIM_EACHFRAME false   // Writes values after each frame of the sim
#endif
