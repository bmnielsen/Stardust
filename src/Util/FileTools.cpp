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

        for (auto &path : dataLoadPaths)
        {
            auto filename = (std::ostringstream() << path << label << "." << fileType).str();
            if (std::filesystem::exists(filename)) return filename;
        }

        return "";
    }
}