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

    bool Powers(BWAPI::TilePosition pylonTile, BWAPI::TilePosition buildingTile, BWAPI::UnitType buildingType)
    {
        int offsetY = buildingTile.y - pylonTile.y;
        int offsetX = buildingTile.x - pylonTile.x;

        if (buildingType.tileWidth() == 4)
        {
            if (offsetY < -5 || offsetY > 4) return false;
            if ((offsetY == -5 || offsetY == 4) && (offsetX < -4 || offsetX > 1)) return false;
            if ((offsetY == -4 || offsetY == 3) && (offsetX < -7 || offsetX > 4)) return false;
            if ((offsetY == -3 || offsetY == 2) && (offsetX < -8 || offsetX > 5)) return false;
            return (offsetX >= -8 && offsetX <= 6);
        }

        if (offsetY < -4 || offsetY > 4) return false;
        if (offsetY == 4 && (offsetX < -3 || offsetX > 2)) return false;
        if ((offsetY == -4 || offsetY == 3) && (offsetX < -6 || offsetX > 5)) return false;
        if ((offsetY == -3 || offsetY == 2) && (offsetX < -7 || offsetX > 6)) return false;
        return (offsetX >= -7 && offsetX <= 7);
    }
}
