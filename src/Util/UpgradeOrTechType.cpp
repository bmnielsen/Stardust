#include "UpgradeOrTechType.h"

int UpgradeOrTechType::mineralPrice() const
{
    if (isTechType())
    {
        return techType.mineralPrice();
    }

    return upgradeType.mineralPrice(BWAPI::Broodwar->self()->getUpgradeLevel(upgradeType) + 1);
}

int UpgradeOrTechType::gasPrice() const
{
    if (isTechType())
    {
        return techType.gasPrice();
    }

    return upgradeType.gasPrice(BWAPI::Broodwar->self()->getUpgradeLevel(upgradeType) + 1);
}

int UpgradeOrTechType::upgradeOrResearchTime() const
{
    if (isTechType())
    {
        return techType.researchTime();
    }

    return upgradeType.upgradeTime(BWAPI::Broodwar->self()->getUpgradeLevel(upgradeType) + 1);
}

BWAPI::UnitType UpgradeOrTechType::whatUpgradesOrResearches() const
{
    if (isTechType())
    {
        return techType.whatResearches();
    }

    return upgradeType.whatUpgrades();
}

int UpgradeOrTechType::currentLevel() const
{
    if (isTechType())
    {
        return BWAPI::Broodwar->self()->hasResearched(techType) ? 1 : 0;
    }

    return BWAPI::Broodwar->self()->getUpgradeLevel(upgradeType) > 0;
}

int UpgradeOrTechType::maxLevel() const
{
    if (isTechType())
    {
        return 1;
    }

    return upgradeType.maxRepeats();
}

bool UpgradeOrTechType::operator==(const UpgradeOrTechType &other)
{
    return upgradeType == other.upgradeType && techType == other.techType;
}

std::ostream &operator<<(std::ostream &os, const UpgradeOrTechType &type)
{
    if (type.isTechType())
    {
        os << type.techType;
    }
    else
    {
        os << type.upgradeType;
    }

    return os;
}
