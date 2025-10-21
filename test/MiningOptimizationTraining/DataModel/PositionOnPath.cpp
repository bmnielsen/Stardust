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
#if USE_VELOCITY_AND_HEADING
           << " dx=" << (int)positionOnPath.velocityX
           << " dy=" << (int)positionOnPath.velocityY
           << " h=" << (unsigned int)positionOnPath.heading
#endif
           << ")";

        return os;
    }

}