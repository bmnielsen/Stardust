#include "Resource.h"

#include "Geo.h"

ResourceImpl::ResourceImpl(BWAPI::Unit unit)
    : id(unit->getID())
    , isMinerals(unit->getType().isMineralField())
    , tile(unit->getTilePosition())
    , center(unit->getPosition())
    , initialAmount(unit->getResources())
    , currentAmount(unit->getResources())
    , seenLastFrame(false)
    , destroyed(false)
{}

bool ResourceImpl::hasMyCompletedRefinery() const
{
    if (isMinerals) return false;
    if (!refinery) return false;
    if (!refinery->completed) return false;
    if (refinery->player != BWAPI::Broodwar->self()) return false;

    return true;
}

BWAPI::Unit ResourceImpl::getBwapiUnitIfVisible() const
{
    if (refinery && refinery->bwapiUnit && refinery->bwapiUnit->isVisible())
    {
        return refinery->bwapiUnit;
    }

    for (auto unit : BWAPI::Broodwar->getNeutralUnits())
    {
        if (!unit->isVisible()) continue;
        if (unit->getTilePosition() != tile) continue;
        if (isMinerals && !unit->getType().isMineralField()) continue;
        if (!isMinerals && unit->getType() != BWAPI::UnitTypes::Resource_Vespene_Geyser) continue;

        return unit;
    }

    return nullptr;
}

int ResourceImpl::getDistance(const Unit &unit) const
{
    return Geo::EdgeToEdgeDistance(
            isMinerals ? BWAPI::UnitTypes::Resource_Mineral_Field : BWAPI::UnitTypes::Resource_Vespene_Geyser,
            center,
            unit->type,
            unit->lastPosition);
}

int ResourceImpl::getDistance(BWAPI::Position pos) const
{
    return Geo::EdgeToPointDistance(
            isMinerals ? BWAPI::UnitTypes::Resource_Mineral_Field : BWAPI::UnitTypes::Resource_Vespene_Geyser,
            center,
            pos);
}

int ResourceImpl::getDistance(const Resource &other) const
{
    return Geo::EdgeToEdgeDistance(
            isMinerals ? BWAPI::UnitTypes::Resource_Mineral_Field : BWAPI::UnitTypes::Resource_Vespene_Geyser,
            center,
            other->isMinerals ? BWAPI::UnitTypes::Resource_Mineral_Field : BWAPI::UnitTypes::Resource_Vespene_Geyser,
            other->center);
}

int ResourceImpl::getDistance(BWAPI::UnitType otherType, BWAPI::Position otherCenter) const
{
    return Geo::EdgeToEdgeDistance(
            isMinerals ? BWAPI::UnitTypes::Resource_Mineral_Field : BWAPI::UnitTypes::Resource_Vespene_Geyser,
            center,
            otherType,
            otherCenter);
}

std::ostream &operator<<(std::ostream &os, const ResourceImpl &resource)
{
    if (resource.isMinerals)
    {
        os << "Minerals";
    }
    else
    {
        os << "Gas";
    }

    os << ":" << resource.id << "@" << BWAPI::WalkPosition(resource.center);

    if (resource.destroyed)
    {
        os << " (destroyed)";
    }
    else
    {
        if (!resource.refinery || resource.refinery->player == BWAPI::Broodwar->self())
        {
            os << " " << resource.currentAmount << "/" << resource.initialAmount;
        }
        if (resource.refinery)
        {
            os << " with refinery " << *resource.refinery;
        }
    }

    return os;
}
