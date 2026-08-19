#include "FileTools.h"

#include <filesystem>

#define DEBUG_FAILED_PATHS false

namespace FileTools
{
    namespace
    {
        std::string overriddenBasePath;

        std::vector<std::string> dataLoadPaths = {
                "bwapi-data/read/",
                "bwapi-data/write/",
                "bwapi-data/AI/"
        };
        std::string dataWritePath = "bwapi-data/write/";
    }

    void overrideBasePath(const std::string &basePath)
    {
        overriddenBasePath = basePath;
    }

    std::string getFilePath(const std::string &label, const std::string &fileType, bool writing)
    {
        if (writing)
        {
            if (overriddenBasePath.empty())
            {
                return (std::ostringstream() << dataWritePath << label << "." << fileType).str();
            }
            return (std::ostringstream() << overriddenBasePath << label << "." << fileType).str();
        }

        std::vector<std::string> pathsToTry;
        if (overriddenBasePath.empty())
        {
            pathsToTry = dataLoadPaths;
        }
        else
        {
            pathsToTry = {overriddenBasePath};
        }

#if DEBUG_FAILED_PATHS
        std::vector<std::string> triedPaths;
#endif
        for (auto &path : pathsToTry)
        {
            auto filename = (std::ostringstream() << path << label << "." << fileType).str();
            if (std::filesystem::exists(filename))
            {
                return filename;
            }
#if DEBUG_FAILED_PATHS
            triedPaths.push_back(filename);
#endif
        }

#if DEBUG_FAILED_PATHS
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