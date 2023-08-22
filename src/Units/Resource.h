#pragma once

#include "Common.h"
#include "Unit.h"

class ResourceImpl;

typedef std::shared_ptr<ResourceImpl> Resource;

class ResourceImpl
{
public:
    int id;
    bool isMinerals;

    BWAPI::TilePosition tile;
    BWAPI::Position center;

    int initialAmount;
    int currentAmount;

    bool destroyed; // For mineral fields, when they are mined out and removed
    Unit refinery;  // For geysers, the refinery unit a player has built on it

    explicit ResourceImpl(BWAPI::Unit unit);

    bool hasMyCompletedRefinery() const;

    BWAPI::Unit getBwapiUnitIfVisible() const;

    int getDistance(const Unit &unit) const;

    int getDistance(BWAPI::Position pos) const;

    int getDistance(const Resource &other) const;

    int getDistance(BWAPI::UnitType type, BWAPI::Position center) const;
};

std::ostream &operator<<(std::ostream &os, const ResourceImpl &resource);
