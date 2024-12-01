#pragma once

#include <BWAPI.h>
#include "MyWorker.h"

struct PositionAndVelocity
{
public:
    int16_t x;
    int16_t y;
    int16_t dx;
    int16_t dy;
    int16_t heading;
    uint32_t previousPositionsHash;

    PositionAndVelocity() : x(-1), y(-1), dx(-1), dy(-1), heading(-1), previousPositionsHash(0) {}

    PositionAndVelocity(int16_t x, int16_t y, int16_t dx, int16_t dy, int16_t heading, uint32_t previousPositionsHash)
        : x(x), y(y), dx(dx), dy(dy), heading(heading), previousPositionsHash(previousPositionsHash) {}

    explicit PositionAndVelocity(const BWAPI::Unit &unit)
            : x(unit->getPosition().x)
            , y(unit->getPosition().y)
            , dx(int16_t(unit->getVelocityX() * 1000.0))
            , dy(int16_t(unit->getVelocityY() * 1000.0))
            , heading(int16_t(unit->getAngle() * 1000.0))
            , previousPositionsHash(0)
    {}

    explicit PositionAndVelocity(const MyWorker &worker, const PositionAndVelocity *previousPosition)
            : x(worker->lastPosition.x)
            , y(worker->lastPosition.y)
            , dx(worker->horizontalKiloSpeed)
            , dy(worker->verticalKiloSpeed)
            , heading(worker->kiloHeading)
            , previousPositionsHash(previousPosition ? previousPosition->getHash() : 0)
    {}

    bool operator==(const PositionAndVelocity &other) const
    {
        return x == other.x
               && y == other.y
               && dx == other.dx
               && dy == other.dy
               && heading == other.heading
               && previousPositionsHash == other.previousPositionsHash;
    }

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

    [[nodiscard]] uint32_t getHash() const
    {
        if (hashComputed) return hash;

        hash = previousPositionsHash;
        auto add = [&](uint32_t val)
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

        hashComputed = true;
        return hash;
    }

    [[nodiscard]] bool speedExceeds(double fractionOfTopSpeed) const;

    static bool tryParse(const std::string &str, PositionAndVelocity &out);

    template <typename S>
    void serialize(S& s) {
        s.value2b(x);
        s.value2b(y);
        s.value2b(dx);
        s.value2b(dy);
        s.value2b(heading);
        s.value4b(previousPositionsHash);
    }

private:
    mutable bool hashComputed = false;
    mutable uint32_t hash = 0;
};

template <>
struct std::hash<PositionAndVelocity>
{
    std::size_t operator()(const PositionAndVelocity& pos) const
    {
        return pos.getHash();
    }
};

std::ostream &operator<<(std::ostream &os, const PositionAndVelocity &positionAndVelocity);
std::ostream &operator<<(std::ostream &os, const std::vector<PositionAndVelocity> &vec);
