#include <BWAPI/PrepareGatherPathResult.h>

#include "bwgame.h"

namespace BWAPI
{
    PrepareGatherPathResult::PrepareGatherPathResult(int returnPathStartFrame,
                                                     ExactPosition returnPathStartPosition,
                                                     std::unique_ptr<bwgame::state> returnPathState)
            : returnPathStartFrame(returnPathStartFrame)
            , returnPathStartPosition(returnPathStartPosition)
            , returnPathState(std::move(returnPathState))
    {}

    PrepareGatherPathResult::~PrepareGatherPathResult() = default;
}
