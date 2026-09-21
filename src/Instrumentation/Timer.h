#pragma once

#include <string>

namespace Timer
{
    void initialize();

    void gameEnd();

    void start(const std::string &label);

    void checkpoint(const std::string &label);

    void stop(bool forceOutput = false);
}
