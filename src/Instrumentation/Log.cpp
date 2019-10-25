#include "Log.h"

#include <BWAPI.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace Log
{
    namespace
    {
        bool isDebugLogging = false;
        bool isOutputtingToConsole = false;
        auto startTime = std::chrono::system_clock::now();
        std::ofstream *log;
        std::ofstream *debugLog;
        std::map<std::string, std::ofstream *> csvFiles;
        std::vector<std::string> logFiles;

        std::string logFileName(const std::string& base, bool csv = false)
        {
            std::ostringstream filename;
            filename << "bwapi-data/write/" << base;
            auto tt = std::chrono::system_clock::to_time_t(startTime);
            auto tm = std::localtime(&tt);
            filename << "_" << std::put_time(tm, "%Y%m%d_%H%M%S");
            if (csv)
                filename << ".csv";
            else
                filename << ".txt";

            logFiles.push_back(filename.str());

            return filename.str();
        }
    }

    LogWrapper::LogWrapper(std::ofstream *logFile, bool outputToConsole, bool csv)
            : os(new std::ostringstream)
            , refCount(new int(1))
            , logFile(logFile)
            , outputToConsole(outputToConsole)
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

    LogWrapper::LogWrapper(const LogWrapper &other)
            : os(other.os)
            , refCount(other.refCount)
            , logFile(other.logFile)
            , outputToConsole(other.outputToConsole)
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
            if (outputToConsole)
            {
                std::cout << os->str() << std::endl;
            }

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

    void SetOutputToConsole(bool outputToConsole)
    {
        isOutputtingToConsole = outputToConsole;
    }

    LogWrapper Get()
    {
        if (!log)
        {
            log = new std::ofstream();
            log->open(logFileName("Locutus_log"), std::ofstream::trunc);
        }

        return LogWrapper(log, isOutputtingToConsole);
    }

    LogWrapper Debug()
    {
        if (!isDebugLogging) return LogWrapper(nullptr, false);

        if (!debugLog)
        {
            debugLog = new std::ofstream();
            debugLog->open(logFileName("Locutus_debug"), std::ofstream::trunc);
        }

        return LogWrapper(debugLog, false);
    }

    LogWrapper Csv(const std::string& name)
    {
        if (!isDebugLogging) return LogWrapper(nullptr, false);

        if (!csvFiles[name])
        {
            csvFiles[name] = new std::ofstream();
            csvFiles[name]->open(logFileName(name, true), std::ofstream::trunc);
        }

        return LogWrapper(csvFiles[name], false, true);
    }

    std::vector<std::string> &LogFiles()
    {
        return logFiles;
    }
}