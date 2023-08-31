#pragma once

#include "Common.h"

class UpgradeOrTechType
{
public:
    BWAPI::UpgradeType upgradeType;
    BWAPI::TechType techType;

    UpgradeOrTechType(BWAPI::UpgradeType upgradeType) : upgradeType(upgradeType), techType(BWAPI::TechTypes::None) {}

    UpgradeOrTechType(BWAPI::TechType techType) : upgradeType(BWAPI::UpgradeTypes::None), techType(techType) {}

    [[nodiscard]] bool isTechType() const { return techType != BWAPI::TechTypes::None; }

    [[nodiscard]] int mineralPrice() const;

    [[nodiscard]] int gasPrice() const;

    [[nodiscard]] int upgradeOrResearchTime() const;

    [[nodiscard]] BWAPI::UnitType whatUpgradesOrResearches() const;

    [[nodiscard]] BWAPI::UnitType whatsRequired() const;

    [[nodiscard]] int currentLevel() const;

    [[nodiscard]] int maxLevel() const;

    bool operator==(const UpgradeOrTechType &other) const;
};

std::ostream &operator<<(std::ostream &os, const UpgradeOrTechType &type);
