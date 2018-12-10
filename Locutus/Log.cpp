#include "Log.h"

#include <BWAPI.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace Log
{
    namespace {
        bool isDebugLogging = false;
        auto startTime = std::chrono::system_clock::now();
        std::ofstream* log;
        std::ofstream* debugLog;
        std::map<std::string, std::ofstream*> csvFiles;

        std::string logFileName(std::string base, bool csv = false)
        {
            std::ostringstream filename;
            filename << "bwapi-data/write/" << base;
            auto tt = std::chrono::system_clock::to_time_t(startTime);
#pragma warning(disable : 4996)
            auto tm = std::localtime(&tt);
            filename << "_" << std::put_time(tm, "%Y%m%d_%H%M%S");
            if (csv)
                filename << ".csv";
            else
                filename << ".txt";
            return filename.str();
        }
    }

    LogWrapper::LogWrapper(std::ofstream* logFile, bool csv)
        : os(new std::ostringstream)
        , refCount(new int(1))
        , logFile(logFile)
        , csv(csv)
        , first(true)
    {
        if (!logFile) return;
        if (csv) return;

        int seconds = BWAPI::Broodwar->getFrameCount() / 24;
        int minutes = seconds / 60;
        seconds = seconds % 60;
        (*os) << BWAPI::Broodwar->getFrameCount() << "(" << minutes << ":" << (seconds < 10 ? "0" : "") << seconds << "): ";
    }

    LogWrapper::LogWrapper(const LogWrapper& other)
        : os(other.os)
        , refCount(other.refCount)
        , logFile(other.logFile)
        , csv(other.csv)
        , first(other.first)
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
            log->open(logFileName("Locutus_log"), std::ofstream::trunc);
        }

        return LogWrapper(log);
    }

    LogWrapper Debug()
    {
        if (!isDebugLogging) return LogWrapper(nullptr);

        if (!debugLog)
        {
            debugLog = new std::ofstream();
            debugLog->open(logFileName("Locutus_log_debug"), std::ofstream::trunc);
        }

        return LogWrapper(debugLog);
    }

    LogWrapper Csv(std::string name)
    {
        if (!csvFiles[name])
        {
            csvFiles[name] = new std::ofstream();
            csvFiles[name]->open(logFileName(name, true), std::ofstream::trunc);
        }

        return LogWrapper(csvFiles[name], true);
    }
}