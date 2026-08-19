#pragma once

#include "Common.h"

namespace FileTools
{
    // Overrides the base path used to generate file paths
    // Useful when tests need to put data somewhere else
    void overrideBasePath(const std::string &basePath);

    // Gets a valid path to the file with the given label
    // If writing, the path is to the write folder
    // If reading, the path is to whichever of the read, write and AI folders the file is found in first
    std::string getFilePath(const std::string &label, const std::string &fileType, bool writing = false);
}
