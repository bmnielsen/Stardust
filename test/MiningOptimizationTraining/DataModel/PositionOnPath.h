#pragma once

#include <cstdint>
#include "MyWorker.h"

#include <BWAPI/ExactPosition.h>

#define USE_VELOCITY true

namespace MiningOptimizationTraining
{
    /*
     * Represents a position on a path.
     *
     * For training, we store subpixel data to better determine when a path is changing because of repathing or just because the subpixels were
     * different earlier in the path. As this data is not available to us outside of our OpenBW development environment, it is not included in the
     * data used by the bot in actual play.
     *
     * TODO: Do some additional testing on whether we need to store velocities and heading - we know that the paths sometimes differ only by these
     *       elements, but maybe we can skip storing them unless we see that the path needs it
     *       We could also consider scaling the velocity and heading to even fewer bits and see if that works - maybe an 8-bit hash of all three
     *
     * The various data fields require the following number of bits:
     * x: 13
     * y: 13
     * velocityX: 8
     * velocityY: 8
     * heading: 8
     * exactPositionDelta: 24 (12 for x and 12 for y)
     */
    class PositionOnPath
    {
    public:
        uint16_t x;         // X pixel position, between 0 and 8191 for the default maximum map size of 256 tiles
        uint16_t y;         // Y pixel position, between 0 and 8191 for the default maximum map size of 256 tiles

        int8_t velocityX;   // 8-bit integer representation of the unit's speed on the X axis
        int8_t velocityY;   // 8-bit integer representation of the unit's speed on the Y axis
        int8_t heading;     // The heading in BW representation (1/256th of a circle)

        BWAPI::ExactPositionDifference exactPositionDelta; // Subpixel delta from previous position on path, values between -1280 and 1280 inclusive

        PositionOnPath()
                : x(0)
                , y(0)
                , velocityX(0)
                , velocityY(0)
                , heading(0)
                , exactPositionDelta({0, 0})
        {}

        explicit PositionOnPath(const BWAPI::Unit &unit)
                : x((uint16_t)unit->getPosition().x)
                , y((uint16_t)unit->getPosition().y)
                , velocityX(MyWorkerImpl::to8bSpeed(unit->getVelocityX()))
                , velocityY(MyWorkerImpl::to8bSpeed(unit->getVelocityY()))
                , heading(unit->getExactPosition().heading)
                , exactPositionDelta({0, 0})
        {}

        PositionOnPath(const BWAPI::Unit &unit, const BWAPI::ExactPosition &previousPosition)
                : x((uint16_t)unit->getPosition().x)
                , y((uint16_t)unit->getPosition().y)
                , velocityX(MyWorkerImpl::to8bSpeed(unit->getVelocityX()))
                , velocityY(MyWorkerImpl::to8bSpeed(unit->getVelocityY()))
                , heading(unit->getExactPosition().heading)
                , exactPositionDelta(unit->getExactPosition() - previousPosition)
        {}

        bool operator==(const PositionOnPath &other) const
        {
#if USE_VELOCITY
            return std::tie(x, y, exactPositionDelta, heading, velocityX, velocityY) ==
                   std::tie(other.x, other.y, other.exactPositionDelta, other.heading, other.velocityX, other.velocityY);
#else
            return std::tie(x, y, exactPositionDelta, heading) == std::tie(other.x, other.y, other.exactPositionDelta, other.heading);
#endif
        }

        bool operator<(const PositionOnPath &other) const
        {
#if USE_VELOCITY
            return std::tie(x, y, exactPositionDelta, heading, velocityX, velocityY) <
                   std::tie(other.x, other.y, other.exactPositionDelta, other.heading, other.velocityX, other.velocityY);
#else
            return std::tie(x, y, exactPositionDelta, heading) < std::tie(other.x, other.y, other.exactPositionDelta, other.heading);
#endif
        }

        [[nodiscard]] BWAPI::Position bwapiPosition() const
        {
            return {x, y};
        }

        template <typename S>
        void serialize(S& s) {
            s.value2b(x);
            s.value2b(y);

            // These could be stored as 2 bytes each but it would be a bit messy and this is only used in training anyway
            s.value4b(exactPositionDelta.x);
            s.value4b(exactPositionDelta.y);

            s.value1b(heading);
#if USE_VELOCITY
            s.value1b(velocityX);
            s.value1b(velocityY);
#endif
        }
    };

    std::ostream &operator<<(std::ostream &os, const PositionOnPath &positionOnPath);
}

template <>
struct std::hash<MiningOptimizationTraining::PositionOnPath>
{
    std::size_t operator()(const MiningOptimizationTraining::PositionOnPath& pos) const
    {
        // Only intended for use in std::unordered_map, so hash quality is not important
        uint32_t xy = (pos.x << 16) + pos.y;
        uint32_t dxySubpixel = ((uint16_t)pos.exactPositionDelta.x << 16) + (uint16_t)pos.exactPositionDelta.y;
        uint32_t velocityAndHeading = (unsigned char)pos.heading;
#if USE_VELOCITY
        velocityAndHeading += ((uint8_t)pos.velocityX << 16) + ((uint8_t)pos.velocityY << 8);
#endif
        return xy ^ dxySubpixel ^ velocityAndHeading;
    }
};
