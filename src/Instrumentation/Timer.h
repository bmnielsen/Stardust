#pragma once

#include <string>

namespace Timer
{
    void start(std::string label);

    void checkpoint(std::string label);

    void stop();
}
