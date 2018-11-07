#include "UnitUtil.h"

namespace UnitUtil
{
    bool IsUndetected(BWAPI::Unit unit)
    {
        return (unit->isCloaked() || unit->getType().hasPermanentCloak()) && !unit->isDetected();
    }

    BWAPI::Position PredictPosition(BWAPI::Unit unit, int frames)
    {
        if (!unit || !unit->exists() || !unit->isVisible()) return BWAPI::Positions::Invalid;

        return unit->getPosition() + BWAPI::Position(frames * unit->getVelocityX(), frames * unit->getVelocityY());
    }
}
