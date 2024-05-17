#include "SquadData.h"

using namespace Locutus;

namespace
{
    Squad emptySquad;
}

SquadData::SquadData() 
{
}

void SquadData::update()
{
	updateAllSquads();
    verifySquadUniqueMembership();
}

void SquadData::clearSquadData()
{
    // give back workers who were in squads
    for (auto & kv : _squads)
	{
        Squad & squad = kv.second;

        const BWAPI::Unitset & units = squad.getUnits();

        for (auto unit : units)
        {
            if (unit->getType().isWorker())
            {
                WorkerManager::Instance().finishedWithWorker(unit);
            }
        }
	}

	_squads.clear();
}

void SquadData::removeSquad(const std::string & squadName)
{
    auto squadPtr = _squads.find(squadName);

    UAB_ASSERT_WARNING(squadPtr != _squads.end(), "Trying to clear a squad that didn't exist: %s", squadName.c_str());
    if (squadPtr == _squads.end())
    {
        return;
    }

    for (const auto unit : squadPtr->second.getUnits())
    {
        if (unit->getType().isWorker())
        {
            WorkerManager::Instance().finishedWithWorker(unit);
        }
    }

    _squads.erase(squadName);
}

const std::map<std::string, Squad> & SquadData::getSquads() const
{
    return _squads;
}

bool SquadData::squadExists(const std::string & squadName) const
{
    return _squads.find(squadName) != _squads.end();
}

void SquadData::addSquad(const Squad & squad)
{
	_squads[squad.getName()] = squad;
}

void SquadData::updateAllSquads()
{
	for (auto & kv : _squads)
	{
		kv.second.update();
	}
}

void SquadData::drawSquadInformation(int x, int y) 
{

}

void SquadData::verifySquadUniqueMembership()
{
    BWAPI::Unitset assigned;

    for (const auto & kv : _squads)
    {
        for (const auto unit : kv.second.getUnits())
        {
            if (assigned.contains(unit))
            {
                //BWAPI::Broodwar->printf("Unit is in at least two squads: %s", unit->getType().getName().c_str());
            }

            assigned.insert(unit);
        }
    }
}

bool SquadData::unitIsInSquad(BWAPI::Unit unit) const
{
    return getUnitSquad(unit) != nullptr;
}

const Squad * SquadData::getUnitSquad(BWAPI::Unit unit) const
{
    for (const auto & kv : _squads)
    {
        if (kv.second.getUnits().contains(unit))
        {
            return &kv.second;
        }
    }

    return nullptr;
}

Squad * SquadData::getUnitSquad(BWAPI::Unit unit)
{
    for (auto & kv : _squads)
    {
        if (kv.second.getUnits().contains(unit))
        {
            return &kv.second;
        }
    }

    return nullptr;
}

void SquadData::assignUnitToSquad(BWAPI::Unit unit, Squad & squad)
{
    UAB_ASSERT_WARNING(canAssignUnitToSquad(unit, squad), "We shouldn't be re-assigning this unit!");

    Squad * previousSquad = getUnitSquad(unit);
    if (previousSquad) previousSquad->removeUnit(unit);

    squad.addUnit(unit);
}

bool SquadData::canAssignUnitToSquad(BWAPI::Unit unit, const Squad & squad) const
{
    const Squad * unitSquad = getUnitSquad(unit);

    // make sure strictly less than so we don't reassign to the same squad etc
    return !unitSquad || (unitSquad->getPriority() < squad.getPriority());
}

Squad & SquadData::getSquad(const std::string & squadName)
{
//    UAB_ASSERT_WARNING(squadExists(squadName), "Trying to access squad that doesn't exist: %s", squadName);
    if (!squadExists(squadName))
    {
        int a = 10;
    }

    return _squads[squadName];
}

const Squad & SquadData::getSquad(const std::string & squadName) const
{
    //UAB_ASSERT_WARNING(squadExists(squadName), "Trying to access squad that doesn't exist: %s", squadName);
    if (!squadExists(squadName))
    {
        int a = 10;
    }

    return _squads.at(squadName);
}

Squad & SquadData::getSquad(const MicroManager * microManager)
{
    for (auto& squad : _squads)
        if (squad.second.hasMicroManager(microManager))
            return squad.second;

    UAB_ASSERT(false, "Getting squad for unowned MicroManager");
    return emptySquad;
}
