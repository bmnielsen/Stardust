#pragma once

#include <iostream>

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

    public:
        LogWrapper(std::ofstream* _logFile);
        LogWrapper(const LogWrapper& other);
        ~LogWrapper();
        
        template<typename T> LogWrapper& operator<<(T const& value) 
        {
            if (logFile) (*os) << value;
            return *this;
        }
    };

    void SetDebug(bool debug);
    LogWrapper Get();
    LogWrapper Debug();
}