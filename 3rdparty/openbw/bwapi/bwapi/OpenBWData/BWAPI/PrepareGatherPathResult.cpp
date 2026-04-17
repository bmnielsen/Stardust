#include <BWAPI/PrepareGatherPathResult.h>

#include "bwgame.h"

namespace BWAPI
{
    PrepareGatherPathResult::PrepareGatherPathResult(int startFrame,
                                                     ExactPosition startPosition,
                                                     std::unique_ptr<bwgame::state> state)
            : startFrame(startFrame)
            , startPosition(startPosition)
            , state(std::move(state))
    {}

    PrepareGatherPathResult::~PrepareGatherPathResult() = default;
}
