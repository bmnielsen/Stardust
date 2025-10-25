#include "PositionOnPath.h"

namespace MiningOptimizationTraining
{
    std::ostream &operator<<(std::ostream &os, const SubpixelPosition &subpixelPosition)
    {
        os << "(" << subpixelPosition.x << "," << subpixelPosition.y << ")";
        return os;
    }

    std::ostream &operator<<(std::ostream &os, const PositionOnPath &positionOnPath)
    {
        os << "(x=" << positionOnPath.x
           << " y=" << positionOnPath.y
           << " dxSpxl=" << positionOnPath.dXSubpixel
           << " dySpxl=" << positionOnPath.dYSubpixel
           << " h=" << (int)positionOnPath.heading
#if USE_VELOCITY
           << " dx=" << (int)positionOnPath.velocityX
           << " dy=" << (int)positionOnPath.velocityY
#endif
           << ")";

        return os;
    }

}