#pragma once

#include <cstdint>
#include "MyWorker.h"
#include "Geo.h"

#define USE_VELOCITY true

namespace MiningOptimizationTraining
{
    // Represends a position at subpixel precision
    struct SubpixelPosition
    {
        uint32_t x;
        uint32_t y;

        explicit SubpixelPosition(BWAPI::Unit unit)
                : x(((uint32_t)unit->getPosition().x << 8) + unit->getSubpixelPosition().x)
                , y(((uint32_t)unit->getPosition().y << 8) + unit->getSubpixelPosition().y)
        {}
    };

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
     * dXSubpixel: 12
     * dYSubpixel: 12
     */
    class PositionOnPath
    {
    public:
        uint16_t x;         // X pixel position, between 0 and 8191 for the default maximum map size of 256 tiles
        uint16_t y;         // Y pixel position, between 0 and 8191 for the default maximum map size of 256 tiles

        int8_t velocityX;   // 8-bit integer representation of the unit's speed on the X axis
        int8_t velocityY;   // 8-bit integer representation of the unit's speed on the Y axis
        int8_t heading;     // The heading in BW representation (1/256th of a circle)

        int16_t dXSubpixel; // X subpixel delta from previous position on path, between -1280 and 1280 inclusive
        int16_t dYSubpixel; // Y subpixel delta from previous position on path, between -1280 and 1280 inclusive

        PositionOnPath()
                : x(0)
                , y(0)
                , velocityX(0)
                , velocityY(0)
                , heading(0)
                , dXSubpixel(0)
                , dYSubpixel(0)
        {}

        explicit PositionOnPath(const BWAPI::Unit &unit)
                : x((uint16_t)unit->getPosition().x)
                , y((uint16_t)unit->getPosition().y)
                , velocityX(MyWorkerImpl::to8bSpeed(unit->getVelocityX()))
                , velocityY(MyWorkerImpl::to8bSpeed(unit->getVelocityY()))
                , heading(Geo::BWHeading(unit->getAngle()))
                , dXSubpixel(0)
                , dYSubpixel(0)
        {}

        PositionOnPath(const BWAPI::Unit &unit, const SubpixelPosition &previousPosition)
                : x((uint16_t)unit->getPosition().x)
                , y((uint16_t)unit->getPosition().y)
                , velocityX(MyWorkerImpl::to8bSpeed(unit->getVelocityX()))
                , velocityY(MyWorkerImpl::to8bSpeed(unit->getVelocityY()))
                , heading(Geo::BWHeading(unit->getAngle()))
                , dXSubpixel(SubpixelPosition(unit).x - previousPosition.x)
                , dYSubpixel(SubpixelPosition(unit).y - previousPosition.x)
        {}

        bool operator==(const PositionOnPath &other) const
        {
            return x == other.x
                   && y == other.y
                   && dXSubpixel == other.dXSubpixel
                   && dYSubpixel == other.dYSubpixel
                   && heading == other.heading
#if USE_VELOCITY
                   && velocityX == other.velocityX
                   && velocityY == other.velocityY
#endif
                ;
        }

        bool operator<(const PositionOnPath &other) const
        {
            return (x < other.x)
                || (x == other.x && y < other.y)
                || (x == other.x && y == other.y && dXSubpixel < other.dXSubpixel)
                || (x == other.x && y == other.y && dXSubpixel == other.dXSubpixel && dYSubpixel < other.dYSubpixel)
                || (x == other.x && y == other.y && dXSubpixel == other.dXSubpixel && dYSubpixel == other.dYSubpixel
                    && heading < other.heading)
#if USE_VELOCITY
                || (x == other.x && y == other.y && dXSubpixel == other.dXSubpixel && dYSubpixel == other.dYSubpixel
                    && heading == other.heading && velocityX < other.velocityX)
                || (x == other.x && y == other.y && dXSubpixel == other.dXSubpixel && dYSubpixel == other.dYSubpixel
                    && heading == other.heading && velocityX == other.velocityX && velocityY < other.velocityY)
#endif
                ;
        }

        BWAPI::Position bwapiPosition() const
        {
            return BWAPI::Position(x, y);
        }

        template <typename S>
        void serialize(S& s) {
            s.value2b(x);
            s.value2b(y);
            s.value2b(dXSubpixel);
            s.value2b(dYSubpixel);
            s.value1b(heading);
#if USE_VELOCITY
            s.value1b(velocityX);
            s.value1b(velocityY);
#endif
        }
    };

    std::ostream &operator<<(std::ostream &os, const SubpixelPosition &subpixelPosition);
    std::ostream &operator<<(std::ostream &os, const PositionOnPath &positionOnPath);
}

template <>
struct std::hash<MiningOptimizationTraining::PositionOnPath>
{
    std::size_t operator()(const MiningOptimizationTraining::PositionOnPath& pos) const
    {
        // Only intended for use in std::unordered_map, so hash quality is not important
        uint32_t xy = (pos.x << 16) + pos.y;
        uint32_t dxySubpixel = ((uint16_t)pos.dXSubpixel << 16) + (uint16_t)pos.dYSubpixel;
        uint32_t velocityAndHeading = (uint32_t)pos.heading;
#if USE_VELOCITY
        velocityAndHeading += ((uint8_t)pos.velocityX << 16) + ((uint8_t)pos.velocityY << 8);
#endif
        return xy ^ dxySubpixel ^ velocityAndHeading;
    }
};
