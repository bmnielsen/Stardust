#pragma once

#include <BWAPI.h>

#if LOGGING_ENABLED
#if INSTRUMENTATION_ENABLED
#define DEBUG_LOGGING_ENABLED true
#endif
#endif

namespace Log
{
    class LogWrapper
    {
    protected:
#if LOGGING_ENABLED
        std::ostringstream *os;
        int *refCount;
        std::ofstream *logFile;
        bool outputToConsole;
        bool csv;
        bool first;
#endif

    public:

#if LOGGING_ENABLED

        LogWrapper(std::ofstream *logFile, bool outputToConsole, bool csv = false);

        LogWrapper(const LogWrapper &other);

        ~LogWrapper();

#endif

        template<typename T> LogWrapper &operator<<(T const &value)
        {
#if LOGGING_ENABLED
            if (logFile)
            {
                if (csv && !first) (*os) << ',';
                (*os) << value;
                first = false;
            }
#endif
            return *this;
        }

        LogWrapper &operator=(const LogWrapper &) = delete;
    };

    void initialize();

    void SetDebug(bool debug);

    void SetOutputToConsole(bool outputToConsole);

    LogWrapper Get();

    LogWrapper Debug();

    LogWrapper Csv(const std::string &name);

    // Returns a list of the paths of all the log files we have written in this game
    std::vector<std::string> &LogFiles();
}