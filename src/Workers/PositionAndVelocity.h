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

    PositionAndVelocity() : x(-1), y(-1), dx(-1), dy(-1), heading(-1) {}

    PositionAndVelocity(int x, int y, int dx, int dy, int heading) : x(x), y(y), dx(dx), dy(dy), heading(heading) {}

    explicit PositionAndVelocity(const BWAPI::Unit &unit)
            : x(unit->getPosition().x)
            , y(unit->getPosition().y)
            , dx(int(unit->getVelocityX() * 1000.0))
            , dy(int(unit->getVelocityY() * 1000.0))
            , heading(int(unit->getAngle() * 1000.0))
    {}

    explicit PositionAndVelocity(const MyWorker &worker)
            : x(worker->lastPosition.x)
            , y(worker->lastPosition.y)
            , dx(worker->horizontalKiloSpeed)
            , dy(worker->verticalKiloSpeed)
            , heading(worker->kiloHeading)
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
        return x == other.x && y == other.y && dx == other.dx && dy == other.dy && heading == other.heading;
    }

    void addToHash(uint32_t &hash) const
    {
        auto add = [&hash](uint32_t val)
        {
            val = ((val >> 16) ^ val) * 0x45d9f3b;
            val = ((val >> 16) ^ val) * 0x45d9f3b;
            val = (val >> 16) ^ val;
            hash ^= val + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        };

        add(x);
        add(y);
        add(dx);
        add(dy);
        add(heading);
    }

    static PositionAndVelocity fromString(const std::string &str);

    friend bool operator<(const PositionAndVelocity &a, const PositionAndVelocity &b)
    {
        return (a.x < b.x) ||
               (a.x == b.x && a.y < b.y) ||
               (a.x == b.x && a.y == b.y && a.dx < b.dx) ||
               (a.x == b.x && a.y == b.y && a.dx == b.dx && a.dy < b.dy) ||
               (a.x == b.x && a.y == b.y && a.dx == b.dx && a.dy == b.dy && a.heading < b.heading);
    }
};

std::ostream &operator<<(std::ostream &os, const PositionAndVelocity &positionAndVelocity);
std::ostream &operator<<(std::ostream &os, const std::vector<PositionAndVelocity> &vec);
