#pragma once

#include <memory>
#include <string>

// Forwards
namespace bwgame
{
    struct state;
}

namespace BWAPI
{
    struct StateCopy
    {
        StateCopy();
        StateCopy(std::unique_ptr<bwgame::state> state);
        ~StateCopy();
        StateCopy& operator=(StateCopy&& other) noexcept;

        std::unique_ptr<bwgame::state> state;

        // Can be used by the application if it wants to attach some descriptive label to the state copy
        std::string label;
    };
}
