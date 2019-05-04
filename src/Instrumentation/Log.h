#pragma once

#include <BWAPI.h>

namespace Log
{
    class LogWrapper
    {
    private:
        LogWrapper& operator =(const LogWrapper&) = delete;

    protected:
        std::ostringstream* os;
        int* refCount;
        std::ofstream* logFile;
        bool outputToConsole;
        bool csv;
        bool first;

    public:
        LogWrapper(std::ofstream* logFile, bool outputToConsole, bool csv = false);
        LogWrapper(const LogWrapper& other);
        ~LogWrapper();
        
        template<typename T> LogWrapper& operator<<(T const& value)
        {
            if (logFile)
            {
                if (csv && !first) (*os) << ',';
                (*os) << value;
                first = false;
            }
            return *this;
        }
    };

    void SetDebug(bool debug);
    void SetOutputToConsole(bool outputToConsole);
    LogWrapper Get();
    LogWrapper Debug();
    LogWrapper Csv(std::string name);
}