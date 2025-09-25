#include "FileTools.h"

#include <filesystem>

namespace FileTools
{
    namespace
    {
        std::vector<std::string> dataLoadPaths = {
                "bwapi-data/read/",
                "bwapi-data/write/",
                "bwapi-data/AI/"
        };
        std::string dataWritePath = "bwapi-data/write/";
    }

    std::string getFilePath(const std::string &label, const std::string &fileType, bool writing)
    {
        if (writing)
        {
            return (std::ostringstream() << dataWritePath << label << "." << fileType).str();
        }

#if LOGGING_ENABLED
        std::vector<std::string> triedPaths;
#endif
        for (auto &path : dataLoadPaths)
        {
            auto filename = (std::ostringstream() << path << label << "." << fileType).str();
            if (std::filesystem::exists(filename))
            {
                return filename;
            }
#if LOGGING_ENABLED
            triedPaths.push_back(filename);
#endif
        }

#if LOGGING_ENABLED
        std::ostringstream dbg;
        dbg << "Failed to find file for " << label << "; tried ";
        std::string sep;
        for (const auto &path : triedPaths)
        {
            dbg << sep << path;
            sep = ", ";
        }
        Log::Get() << dbg.str();
#endif

        return "";
    }
}