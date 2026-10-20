#pragma once

#include "Common.h"

namespace FileTools
{
    /* Gets a valid path to the file with the given label */
    /* If writing, the path is to the write folder */
    /* If reading, the path is to whichever of the read, write and AI folders the file is found in first */
    std::string getFilePath(const std::string &label, const std::string &fileType, bool writing = false);

    /* Appends one line to <label>.txt in the write folder, opening and closing */
    /* the file per line (release-build diagnostics channel: AppLog is compiled */
    /* out when LOGGING_ENABLED is undefined, so measurement code writes its own */
    /* append-only file via the standard write-dir contract) */
    void appendLine(const std::string &label, const std::string &line);
}
