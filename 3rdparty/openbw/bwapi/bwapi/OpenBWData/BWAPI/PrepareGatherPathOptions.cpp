#include <BWAPI/PrepareGatherPathOptions.h>

#include "bwgame.h"

namespace BWAPI
{
    namespace
    {
        std::unique_ptr<bwgame::state> emptyStartingState;
    }

    PrepareGatherPathOptions::PrepareGatherPathOptions(ExactPosition startPosition)
        : startPosition(startPosition)
        , startingState(emptyStartingState) {}

    PrepareGatherPathOptions::PrepareGatherPathOptions(ExactPosition startPosition,
                                                       const std::unique_ptr<bwgame::state> &startingState)
        : startPosition(startPosition)
        , startingState(startingState) {}

    PrepareGatherPathOptions::~PrepareGatherPathOptions() = default;

    PrepareGatherPathOptions &PrepareGatherPathOptions::prepareReturnFrom(size_t _patchUnitIndex)
    {
        prepareReturn = true;
        patchUnitIndex = _patchUnitIndex;
        return *this;
    }
}
