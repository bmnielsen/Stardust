#pragma once

#if LOGGING_ENABLED

// Keeps track of high-level statistics about mining optimization, like what percentage of collections had path coverage, collision rates, etc.
#define OUTPUT_STATISTICS true

#endif

#if INSTRUMENTATION_ENABLED_VERBOSE

// Logs pathing-related data for each worker, like when they acquire a path and what they have planned to execute
#define VERBOSE_PATH_LOGGING false

// Logs information related to gather takeover
#define VERBOSE_TAKEOVER_LOGGING false

#endif
