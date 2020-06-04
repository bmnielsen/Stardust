#pragma once

#include "Common.h"
#include <bwem.h>

#include "Choke.h"
#include "UnitCluster.h"

class MapSpecificOverride
{
public:
    virtual bool hasMineralWalking() { return false; }

    virtual void initializeChokes(std::map<const BWEM::ChokePoint *, Choke *> &chokes) {}

    // Whether the given unit type can be pathed by BWEM on this map
    virtual bool canUseBwemPath(BWAPI::UnitType unitType)
    {
        // Assumes mineral walking chokes are marked as blocked by BWEM, so workers
        // need special pathing but other units behave correctly
        return !hasMineralWalking() || !unitType.isWorker();
    }

    virtual void onUnitDestroy(BWAPI::Unit unit) {}

    // Hook to perform special pathing for clusters moving. Returns true if the map-specific override has handled the move.
    virtual bool clusterMove(UnitCluster &cluster, BWAPI::Position targetPosition)
    {
        return false;
    }

    virtual void addMainBaseBuildingPlacementAreas(std::set<const BWEM::Area *> &areas) {}
};
