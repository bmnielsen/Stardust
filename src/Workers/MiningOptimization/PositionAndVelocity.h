#pragma once

#include <BWAPI.h>
#include "MyWorker.h"

#include <cppcrc.h>

struct PositionAndVelocity
{
public:
    uint16_t x;
    uint16_t y;
    int8_t dx;
    int8_t dy;
    uint8_t heading;

    PositionAndVelocity() : x(0), y(0), dx(0), dy(0), heading(UINT8_MAX) {}

    PositionAndVelocity(uint16_t x, uint16_t y, int8_t dx, int8_t dy, uint8_t heading)
        : x(x), y(y), dx(dx), dy(dy), heading(heading) {}

    explicit PositionAndVelocity(const BWAPI::Unit &gameUnit)
            : x((uint16_t)gameUnit->getPosition().x)
            , y((uint16_t)gameUnit->getPosition().y)
            , dx(MyWorkerImpl::to8bSpeed(gameUnit->getVelocityX()))
            , dy(MyWorkerImpl::to8bSpeed(gameUnit->getVelocityY()))
            , heading(MyWorkerImpl::to8bHeading(gameUnit->getAngle()))
    {}

    explicit PositionAndVelocity(const OwnedWorker &wrk)
            : x((uint16_t)wrk->prevPos.x)
            , y((uint16_t)wrk->prevPos.y)
            , dx(wrk->horizontalSpeed8b)
            , dy(wrk->verticalSpeed8b)
            , heading(wrk->heading8b)
    {}

    bool operator==(const PositionAndVelocity &other) const
    {
        return x == other.x
               && y == other.y
               && dx == other.dx
               && dy == other.dy
               && heading == other.heading;
    }

    bool operator<(const PositionAndVelocity &other) const
    {
        return (x < other.x) ||
               (x == other.x && y < other.y) ||
               (x == other.x && y == other.y && dx < other.dx) ||
               (x == other.x && y == other.y && dx == other.dx && dy < other.dy) ||
               (x == other.x && y == other.y && dx == other.dx && dy == other.dy && heading < other.heading);
    }

    [[nodiscard]] bool isValid() const
    {
        return heading != UINT8_MAX;
    }

    [[nodiscard]] bool positionEquals(const BWAPI::Unit &gameUnit) const
    {
        return x == gameUnit->getPosition().x
               && y == gameUnit->getPosition().y;
    }

    [[nodiscard]] bool positionEquals(const OwnedWorker &wrk) const
    {
        return x == wrk->prevPos.x
               && y == wrk->prevPos.y;
    }

    [[nodiscard]] BWAPI::Position pt() const
    {
        return {x, y};
    }

    [[nodiscard]] uint16_t getHash() const
    {
        if (hashComputed) return hash;

        uint8_t data[7];
        (uint16_t&)(data[0]) = x;
        (uint16_t&)(data[2]) = y;
        data[4] = dx;
        data[5] = dy;
        data[6] = heading;

        hash = CRC16::CCITT_FALSE::calc(data, 7);

        hashComputed = true;
        return hash;
    }

    /* Determines if the given position is a stable arrival position in a position history approaching a patch or depot */
    static bool isStableArrivalPosition(
            const std::vector<std::shared_ptr<const PositionAndVelocity>> &positions,
            std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator positionIt);

    static bool tryParse(const std::string &str, PositionAndVelocity &out);

    template <typename S>
    void serialize(S& s) {
        s.value2b(x);
        s.value2b(y);
        s.value1b(dx);
        s.value1b(dy);
        s.value1b(heading);
    }

private:
    mutable bool hashComputed = false;
    mutable uint16_t hash = 0;
};

template <>
struct std::hash<PositionAndVelocity>
{
    std::size_t operator()(const PositionAndVelocity& pt) const
    {
        return pt.getHash();
    }
};

std::ostream &operator<<(std::ostream &os, const PositionAndVelocity &positionAndVelocity);
std::ostream &operator<<(std::ostream &os, const std::vector<PositionAndVelocity> &vec);
