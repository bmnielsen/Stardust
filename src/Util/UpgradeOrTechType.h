#pragma once

#include "Common.h"

class UpgradeOrTechType
{
public:
    BWAPI::UpgradeType upgradeType;
    BWAPI::TechType techType;

    UpgradeOrTechType(BWAPI::UpgradeType upgradeType) : upgradeType(upgradeType), techType(BWAPI::TechTypes::None) {}

    UpgradeOrTechType(BWAPI::TechType techType) : upgradeType(BWAPI::UpgradeTypes::None), techType(techType) {}

    bool isTechType() const { return techType != BWAPI::TechTypes::None; }

    int mineralPrice() const;

    int gasPrice() const;

    int upgradeOrResearchTime() const;

    BWAPI::UnitType whatUpgradesOrResearches() const;

    BWAPI::UnitType whatsRequired() const;

    int currentLevel() const;

    int maxLevel() const;

    bool operator==(const UpgradeOrTechType &other);
};

std::ostream &operator<<(std::ostream &os, const UpgradeOrTechType &type);
