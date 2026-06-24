#pragma once

#include "Common.h"
#include "Unit.h"

class ResourceImpl;

typedef std::shared_ptr<ResourceImpl> Resource;
typedef std::shared_ptr<const ResourceImpl> ConstResource;

class ResourceImpl : public std::enable_shared_from_this<ResourceImpl>
{
public:
    ResourceImpl (const ResourceImpl&) = delete;
    ResourceImpl &operator=(const ResourceImpl&) = delete;

    int id;
    bool isMinerals;

    BWAPI::TilePosition tile;
    BWAPI::Position center;

    int initialAmount;
    int currentAmount;

    bool seenLastFrame; // For tracking when mineral fields are mined out and removed
    bool destroyed; // For mineral fields, when they are mined out and removed
    Unit refinery;  // For geysers, the refinery unit a player has built on it

    // For mineral fields, the other mineral fields that workers might try to switch to
    std::set<Resource> resourcesInSwitchPatchRange;

    explicit ResourceImpl(BWAPI::Unit unit);

    [[nodiscard]] bool hasMyCompletedRefinery() const;

    [[nodiscard]] BWAPI::Unit getBwapiUnitIfVisible() const;

    [[nodiscard]] int getDistance(const Unit &unit) const;

    [[nodiscard]] int getDistance(BWAPI::Position pos) const;

    [[nodiscard]] int getDistance(const Resource &other) const;

    [[nodiscard]] int getDistance(BWAPI::UnitType type, BWAPI::Position center) const;

    void update();

private:
    mutable BWAPI::Unit bwapiUnit;
};

std::ostream &operator<<(std::ostream &os, const ResourceImpl &resource);
