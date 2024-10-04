#pragma once

#include <BWAPI.h>
#include "MyWorker.h"

struct PositionAndVelocity
{
    int x;
    int y;
    int dx;
    int dy;
    int heading;
    uint32_t previousPositionsHash;

    PositionAndVelocity() : x(-1), y(-1), dx(-1), dy(-1), heading(-1), previousPositionsHash(0) {}

    PositionAndVelocity(int x, int y, int dx, int dy, int heading, uint32_t previousPositionsHash)
        : x(x), y(y), dx(dx), dy(dy), heading(heading), previousPositionsHash(previousPositionsHash) {}

    explicit PositionAndVelocity(const BWAPI::Unit &unit)
            : x(unit->getPosition().x)
            , y(unit->getPosition().y)
            , dx(int(unit->getVelocityX() * 1000.0))
            , dy(int(unit->getVelocityY() * 1000.0))
            , heading(int(unit->getAngle() * 1000.0))
            , previousPositionsHash(0)
    {}

    explicit PositionAndVelocity(const MyWorker &worker, const PositionAndVelocity *previousPosition)
            : x(worker->lastPosition.x)
            , y(worker->lastPosition.y)
            , dx(worker->horizontalKiloSpeed)
            , dy(worker->verticalKiloSpeed)
            , heading(worker->kiloHeading)
            , previousPositionsHash(previousPosition ? previousPosition->nextHash() : 0)
    {}

    [[nodiscard]] bool isValid() const
    {
        return x != -1;
    }

    [[nodiscard]] bool positionEquals(const BWAPI::Unit &unit) const
    {
        return x == unit->getPosition().x
               && y == unit->getPosition().y;
    }

    [[nodiscard]] bool positionEquals(const MyWorker &worker) const
    {
        return x == worker->lastPosition.x
               && y == worker->lastPosition.y;
    }

    [[nodiscard]] bool equals(const PositionAndVelocity &other) const
    {
        return x == other.x
               && y == other.y
               && dx == other.dx
               && dy == other.dy
               && heading == other.heading
               && previousPositionsHash == other.previousPositionsHash;
    }

    [[nodiscard]] bool positionAndVelocityEquals(const PositionAndVelocity &other) const
    {
        return x == other.x
               && y == other.y
               && dx == other.dx
               && dy == other.dy
               && heading == other.heading;
    }

    [[nodiscard]] BWAPI::Position pos() const
    {
        return {x, y};
    }

    [[nodiscard]] uint32_t nextHash() const
    {
        uint32_t result = previousPositionsHash;
        auto add = [&result](uint32_t val)
        {
            val = ((val >> 16) ^ val) * 0x45d9f3b;
            val = ((val >> 16) ^ val) * 0x45d9f3b;
            val = (val >> 16) ^ val;
            result ^= val + 0x9e3779b9 + (result << 6) + (result >> 2);
        };

        add(x);
        add(y);
        add(dx);
        add(dy);
        add(heading);

        return result;
    }

    static bool isValidString(const std::string &str);
    static PositionAndVelocity fromString(const std::string &str);

    friend bool operator<(const PositionAndVelocity &a, const PositionAndVelocity &b)
    {
        return (a.x < b.x) ||
               (a.x == b.x && a.y < b.y) ||
               (a.x == b.x && a.y == b.y && a.dx < b.dx) ||
               (a.x == b.x && a.y == b.y && a.dx == b.dx && a.dy < b.dy) ||
               (a.x == b.x && a.y == b.y && a.dx == b.dx && a.dy == b.dy && a.heading < b.heading) ||
               (a.x == b.x && a.y == b.y && a.dx == b.dx && a.dy == b.dy && a.heading == b.heading
                    && a.previousPositionsHash < b.previousPositionsHash);
    }
};

std::ostream &operator<<(std::ostream &os, const PositionAndVelocity &positionAndVelocity);
std::ostream &operator<<(std::ostream &os, const std::vector<PositionAndVelocity> &vec);
