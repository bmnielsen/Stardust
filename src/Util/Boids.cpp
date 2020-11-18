#include "Boids.h"

#include "Geo.h"

namespace Boids
{
    void AddSeparation(const UnitImpl *unit, const Unit &other, double detectionLimitFactor, double weight, int &separationX, int &separationY)
    {
        auto dist = unit->getDistance(other);
        double detectionLimit = std::max(unit->type.width(), other->type.width()) * detectionLimitFactor;
        if (dist >= (int) detectionLimit) return;

        // We are within the detection limit
        // Push away with maximum force at 0 distance, no force at detection limit
        double distFactor = 1.0 - (double) dist / detectionLimit;
        int centerDist = Geo::ApproximateDistance(unit->lastPosition.x, other->lastPosition.x, unit->lastPosition.y, other->lastPosition.y);
        if (centerDist == 0) return;
        double scalingFactor = distFactor * distFactor * weight / centerDist;
        separationX -= (int) ((double) (other->lastPosition.x - unit->lastPosition.x) * scalingFactor);
        separationY -= (int) ((double) (other->lastPosition.y - unit->lastPosition.y) * scalingFactor);
    }
}