#include "Log.h"

#if LOGGING_ENABLED

#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>

#endif

namespace Log
{
#if LOGGING_ENABLED
    namespace
    {
        bool isOutputtingToConsole = false;
        std::chrono::system_clock::time_point startTime;
        std::ofstream *log;
#if DEBUG_LOGGING_ENABLED
        bool isDebugLogging;
        std::ofstream *debugLog;
        std::map<std::string, std::ofstream *> csvFiles;
#endif
        std::vector<std::string> logFiles;

        std::string logFileName(const std::string &base, bool csv = false)
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

        int seconds = currentFrame / 24;
        int minutes = seconds / 60;
        seconds = seconds % 60;
        (*os) << currentFrame << "(" << minutes << ":" << (seconds < 10 ? "0" : "") << seconds << "): ";
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

    void initialize()
    {
        startTime = std::chrono::system_clock::now();

        try
        {
            if (log)
            {
                log->close();
                log = nullptr;
            }

#if DEBUG_LOGGING_ENABLED
            isDebugLogging = false;
            if (debugLog)
            {
                debugLog->close();
                debugLog = nullptr;
            }

            for (auto &csvFile : csvFiles)
            {
                if (csvFile.second)
                {
                    csvFile.second->close();
                }
            }

            csvFiles.clear();
#endif
        }
        catch (std::exception &ex)
        {
            // Ignore
        }

        logFiles.clear();
    }

    void SetDebug(bool debug)
    {
#if DEBUG_LOGGING_ENABLED
        isDebugLogging = debug;
#endif
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
            log->open(logFileName("Stardust_log"), std::ofstream::trunc);
        }

        return LogWrapper(log, isOutputtingToConsole);
    }

    LogWrapper Debug()
    {
#if DEBUG_LOGGING_ENABLED
        if (!isDebugLogging) return LogWrapper(nullptr, false);

        if (!debugLog)
        {
            debugLog = new std::ofstream();
            debugLog->open(logFileName("Stardust_debug"), std::ofstream::trunc);
        }

        return LogWrapper(debugLog, false);
#else
        return LogWrapper(nullptr, false);
#endif
    }

    LogWrapper Csv(const std::string &name)
    {
#if DEBUG_LOGGING_ENABLED
        if (!isDebugLogging) return LogWrapper(nullptr, false);

        if (!csvFiles[name])
        {
            csvFiles[name] = new std::ofstream();
            csvFiles[name]->open(logFileName(name, true), std::ofstream::trunc);
        }

        return LogWrapper(csvFiles[name], false, true);
#else
        return LogWrapper(nullptr, false);
#endif
    }

    std::vector<std::string> &LogFiles()
    {
        return logFiles;
    }

#else

    namespace
    {
        std::vector<std::string> logFiles;
    }

    void initialize()
    {
    }

    void SetDebug(bool)
    {
    }

    void SetOutputToConsole(bool)
    {
    }

    LogWrapper Get()
    {
        return LogWrapper();
    }

    LogWrapper Debug()
    {
        return LogWrapper();
    }

    LogWrapper Csv(const std::string &)
    {
        return LogWrapper();
    }

    std::vector<std::string> &LogFiles()
    {
        return logFiles;
    }
#endif
}