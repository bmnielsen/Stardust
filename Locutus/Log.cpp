#include "Log.h"

#include <BWAPI.h>
#include <fstream>

namespace Log
{
    namespace {
        bool isDebugLogging = false;
        std::ofstream* log;
        std::ofstream* debugLog;
    }

    LogWrapper::LogWrapper(std::ofstream* _logFile)
        : os(new std::ostringstream)
        , refCount(new int(1))
        , logFile(_logFile)
    {
        if (!logFile) return;

        int seconds = BWAPI::Broodwar->getFrameCount() / 24;
        int minutes = seconds / 60;
        seconds = seconds % 60;
        (*os) << BWAPI::Broodwar->getFrameCount() << "(" << minutes << ":" << (seconds < 10 ? "0" : "") << seconds << "): ";
    }

    LogWrapper::LogWrapper(const LogWrapper& other)
        : os(other.os)
        , refCount(other.refCount)
        , logFile(other.logFile)
    {
        ++*refCount;
    }

    LogWrapper::~LogWrapper()
    {
        --*refCount;

        if (*refCount == 0)
        {
            if (logFile)
            {
                (*os) << "\n";
                (*logFile) << os->str();
                logFile->flush();
            }

            delete os;
            delete refCount;
        }
    }

    void SetDebug(bool debug)
    {
        isDebugLogging = debug;
    }

    LogWrapper Get()
    {
        if (!log)
        {
            log = new std::ofstream();
            log->open("bwapi-data/write/Locutus_log.txt", std::ofstream::app);
        }

        return LogWrapper(log);
    }

    LogWrapper Debug()
    {
        if (!isDebugLogging) return LogWrapper(nullptr);

        if (!debugLog)
        {
            debugLog = new std::ofstream();
            debugLog->open("bwapi-data/write/Locutus_log_debug.txt", std::ofstream::app);
        }

        return LogWrapper(debugLog);
    }
}