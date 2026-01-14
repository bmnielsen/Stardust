#include <BWAPI/PrepareGatherPathOptions.h>

#include "bwgame.h"

namespace BWAPI
{
    namespace
    {
        std::unique_ptr<bwgame::state> emptyStartingState;
    }

    PrepareGatherPathOptions::PrepareGatherPathOptions(ExactPosition startPosition, size_t patchUnitIndex)
        : startPosition(startPosition)
        , patchUnitIndex(patchUnitIndex)
        , startingState(emptyStartingState) {}

    PrepareGatherPathOptions::PrepareGatherPathOptions(ExactPosition startPosition,
                                                       size_t patchUnitIndex,
                                                       const std::unique_ptr<bwgame::state> &startingState)
        : startPosition(startPosition)
        , patchUnitIndex(patchUnitIndex)
        , startingState(startingState) {}

    PrepareGatherPathOptions::~PrepareGatherPathOptions() = default;
}
