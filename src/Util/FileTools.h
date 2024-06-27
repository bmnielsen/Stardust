#pragma once

#include "Common.h"

namespace FileTools
{
    // Gets a valid path to the file with the given label
    // If writing, the path is to the write folder
    // If reading, the path is to whichever of the read, write and AI folders the file is found in first
    std::string getFilePath(const std::string &label, const std::string &fileType, bool writing = false);
}
