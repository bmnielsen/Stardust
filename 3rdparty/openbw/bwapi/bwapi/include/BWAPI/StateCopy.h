#pragma once

#include <memory>

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
    };
}
