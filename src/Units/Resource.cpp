#include "Resource.h"

ResourceImpl::ResourceImpl(BWAPI::Unit unit)
    : id(unit->getID())
    , isMinerals(unit->getType().isMineralField())
    , tile(unit->getTilePosition())
    , center(unit->getPosition())
    , initialAmount(unit->getResources())
    , currentAmount(unit->getResources())
    , destroyed(false)
{}

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
