#pragma once

#include <BWAPI.h>

struct PositionAndVelocity
{
    int x;
    int y;
    int dx;
    int dy;
    int heading;

    PositionAndVelocity(int x, int y, int dx, int dy, int heading) : x(x), y(y), dx(dx), dy(dy), heading(heading) {}

    explicit PositionAndVelocity(const BWAPI::Unit &unit)
            : x(unit->getPosition().x)
            , y(unit->getPosition().y)
            , dx(int(unit->getVelocityX() * 1000.0))
            , dy(int(unit->getVelocityY() * 1000.0))
            , heading(int(unit->getAngle() * 1000.0))
    {}

    [[nodiscard]] bool positionEquals(const BWAPI::Unit &unit) const
    {
        return x == unit->getPosition().x
               && y == unit->getPosition().y;
    }

    [[nodiscard]] bool equals(const PositionAndVelocity &other) const
    {
        return x == other.x && y == other.y && dx == other.dx && dy == other.dy && heading == other.heading;
    }

    static PositionAndVelocity fromString(const std::string &str);
};

std::ostream &operator<<(std::ostream &os, const PositionAndVelocity &positionAndVelocity);
std::ostream &operator<<(std::ostream &os, const std::vector<PositionAndVelocity> &vec);
std::ostream &operator<<(std::ostream &os, const std::vector<BWAPI::Position> &vec);
