#pragma once

#include "Common.h"
#include "Unit.h"

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
};

typedef std::shared_ptr<ResourceImpl> Resource;

std::ostream &operator<<(std::ostream &os, const ResourceImpl &resource);
