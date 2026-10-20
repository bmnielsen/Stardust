#include "FileTools.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#define DEBUG_FAILED_PATHS false

namespace FileTools
{
    namespace
    {
        std::vector<std::string> dataLoadPaths = {
                "bwapi-data/read/",
                "bwapi-data/write/",
                "bwapi-data/AI/",
                "bwapi-data/"
        };
        std::string dataWritePath = "bwapi-data/write/";
    }

    std::string getFilePath(const std::string &label, const std::string &fileType, bool writing)
    {
        if (writing)
        {
            return (std::ostringstream() << dataWritePath << label << "." << fileType).str();
        }

#if DEBUG_FAILED_PATHS
        std::vector<std::string> triedPaths;
#endif
        for (auto &path : dataLoadPaths)
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
        AppLog::Get() << dbg.str();
#endif

        return "";
    }

    void appendLine(const std::string &label, const std::string &line)
    {
        std::ofstream out(getFilePath(label, "txt", true), std::ofstream::app);
        if (out.is_open()) out << line << "\n";
    }
}