#include "Base.h"

using namespace Locutus;

// For setting base.id on initialization.
// The first base gets base id 1.
static int BaseID = 1;

// Create a base with a location but without resources.
// TODO to be removed - used temporarily by InfoMan
Base::Base(BWAPI::TilePosition pos)
	: id(BaseID)
	, tilePosition(pos)
	, distances(pos)
	, resourceDepot(nullptr)
	, owner(BWAPI::Broodwar->neutral())
	, ownedSince(0)
	, lastScouted(0)
	, spiderMined(false)
	, requiresMineralWalkFromEnemyStartLocations(false)
{
	++BaseID;
}

// Create a base given its position and a set of resources that may belong to it.
// The caller is responsible for eliminating resources which are too small to be worth it.
Base::Base(BWAPI::TilePosition pos, const BWAPI::Unitset availableResources)
	: id(BaseID)
	, tilePosition(pos)
	, distances(pos)
	, resourceDepot(nullptr)
	, owner(BWAPI::Broodwar->neutral())
    , ownedSince(0)
    , lastScouted(0)
    , spiderMined(false)
    , requiresMineralWalkFromEnemyStartLocations(false)
{
	DistanceMap resourceDistances(pos, BaseResourceRange, false);

	for (BWAPI::Unit resource : availableResources)
	{
		if (resource->getInitialTilePosition().isValid() && resourceDistances.getStaticUnitDistance(resource) >= 0)
		{
			if (resource->getType().isMineralField())
			{
				minerals.insert(resource);
			}
			else if (resource->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
			{
				geysers.insert(resource);
			}
		}
	}

	++BaseID;
}

// Called from InformationManager to work around a bug related to BWAPI 4.1.2.
// TODO is this still needed and correct?
void Base::findGeysers()
{
	for (auto unit : BWAPI::Broodwar->getNeutralUnits())
	{
		if ((unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser || unit->getType().isRefinery()) &&
			unit->getPosition().isValid() &&
			unit->getDistance(getPosition()) < 320)
		{
			geysers.insert(unit);
		}
	}
}

// The depot may be null. (That's why player is a separate argument, not depot->getPlayer().)
// A null depot for an owned base means that the base is inferred and hasn't been seen.
void Base::setOwner(BWAPI::Unit depot, BWAPI::Player player)
{
	resourceDepot = depot;
    if (player != owner)
    {
        owner = player;
        ownedSince = BWAPI::Broodwar->getFrameCount();
        spiderMined = false;
    }
}

int Base::getInitialMinerals() const
{
	int total = 0;
	for (const BWAPI::Unit min : minerals)
	{
		total += min->getInitialResources();
	}
	return total;
}

int Base::getInitialGas() const
{
	int total = 0;
	for (const BWAPI::Unit gas : geysers)
	{
		total += gas->getInitialResources();
	}
	return total;
}

void Base::drawBaseInfo() const
{

}