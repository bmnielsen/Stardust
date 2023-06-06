#pragma once

#include "Common.h"
#include "UpgradeOrTechType.h"

class UpgradeProductionGoal
{
public:
    explicit UpgradeProductionGoal(std::string requester, UpgradeOrTechType type, int level = 1, int producerLimit = 1, int frame = 0)
            : requester(std::move(requester))
            , type(type)
            , level(level)
            , producerLimit(producerLimit)
            , frame(frame) {}

    std::string requester;

    // The upgrade type
    [[nodiscard]] UpgradeOrTechType upgradeType() const { return type; }

    // Prerequisite for the next upgrade level
    [[nodiscard]] BWAPI::UnitType prerequisiteForNextLevel() const;

    // Maximum cap of how many producers of the item we should create
    // May be -1 if we do not want to limit it
    [[nodiscard]] int getProducerLimit() const { return producerLimit; }

    // The frame when the upgrade should be started
    [[nodiscard]] int getFrame() const { return frame; };

    friend std::ostream &operator<<(std::ostream &os, const UpgradeProductionGoal &goal)
    {
        os << goal.type << "@" << goal.level << " (" << goal.requester << ")";
        return os;
    }

private:
    UpgradeOrTechType type;
    int level;
    int producerLimit;
    int frame;
};
