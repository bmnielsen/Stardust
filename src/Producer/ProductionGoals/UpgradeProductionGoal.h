#pragma once

#include "Common.h"
#include "UpgradeOrTechType.h"

class UpgradeProductionGoal
{
public:
    explicit UpgradeProductionGoal(UpgradeOrTechType type, int level = 1, int producerLimit = 1)
            : type(type)
            , level(level)
            , producerLimit(producerLimit) {}

    // The upgrade type
    [[nodiscard]] UpgradeOrTechType upgradeType() const { return type; }

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
    UpgradeOrTechType type;
    int level;
    int producerLimit;
};
