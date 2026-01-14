#include <BWAPI/StateCopy.h>

#include "bwgame.h"

namespace BWAPI
{
    StateCopy::StateCopy() = default;
    StateCopy::StateCopy(std::unique_ptr<bwgame::state> state)
            : state(std::move(state))
    {}

    StateCopy::~StateCopy() = default;

    StateCopy& StateCopy::operator=(StateCopy&& other) noexcept
    {
        state = std::move(other.state);
        return *this;
    }
}
