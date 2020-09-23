#pragma once

#include "Common.h"

class UpgradeProductionGoal
{
public:
    explicit UpgradeProductionGoal(BWAPI::UpgradeType type, int level = 1, int producerLimit = 1)
            : type(type)
            , level(level)
            , producerLimit(producerLimit) {}

    // The upgrade type
    [[nodiscard]] BWAPI::UpgradeType upgradeType() const { return type; }

    // Prerequisite for the next upgrade level
    [[nodiscard]] BWAPI::UnitType prerequisiteForNextLevel() const;

    // Maximum cap of how many producers of the item we should create
    // May be -1 if we do not want to limit it
    [[nodiscard]] int getProducerLimit() const { return producerLimit; }

    friend std::ostream &operator<<(std::ostream &os, const UpgradeProductionGoal &goal)
    {
        os << goal.type << "@" << goal.level;
        return os;
    }

private:
    BWAPI::UpgradeType type;
    int level;
    int producerLimit;
};
