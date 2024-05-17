#include "UABAssert.h"
#include "Config.h"

using namespace Locutus;

namespace Locutus
{
namespace Assert
{
    std::string lastErrorMessage;

    const std::string currentDateTime() 
    {
        return "unknown";
    }

    void ReportFailure(const char * condition, const char * file, int line, const char * msg, ...)
    {
        char messageBuffer[1024] = "";
        if (msg != nullptr)
        {
            va_list args;
            va_start(args, msg);
            //vsprintf(messageBuffer, msg, args);
			vsnprintf(messageBuffer, 1024, msg, args);
            va_end(args);
        }

        std::stringstream ss;
        ss                                              << std::endl;
        ss << "!Assert:   " << condition                << std::endl;
        ss << "File:      " << file                     << std::endl;
        ss << "Message:   " << messageBuffer            << std::endl;
        ss << "Line:      " << line                     << std::endl;
        ss << "Time:      " << currentDateTime()        << std::endl;
        
        lastErrorMessage = messageBuffer;

        std::cerr << ss.str();
        //BWAPI::Broodwar->printf("%s", ss.str().c_str());

        if (Config::Debug::LogAssertToErrorFile)
        {
            Logger::LogAppendToFile(Config::Debug::ErrorLogFilename, ss.str());
        }
    }
}
}

