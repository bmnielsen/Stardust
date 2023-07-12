#pragma once

#include "Common.h"
#include <bwem.h>

#include "Choke.h"
#include "UnitCluster.h"
#include "StrategyEngine.h"
#include "ForgeGatewayWall.h"

class MapSpecificOverride
{
public:
    virtual ~MapSpecificOverride() {}

    virtual bool hasMineralWalking() { return false; }

    virtual bool hasAttackClearableChokes() { return false; }

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

    virtual void modifyMainBaseBuildingPlacementAreas(std::set<const BWEM::Area *> &areas) {}

    virtual std::unique_ptr<StrategyEngine> createStrategyEngine()
    {
        return nullptr;
    }

    virtual bool allowDiagonalPathingThrough(int x, int y)
    {
        return false;
    }

    // Hook to do special processing when we know the enemy's starting main base
    virtual void enemyStartingMainDetermined() {}

    // Whether the natural base is behind the main base relative to the enemy starting position
    virtual bool hasBackdoorNatural() { return false; }

    // Adds island areas that aren't marked as such by BWEM
    virtual void addIslandAreas(std::set<const BWEM::Area *> &islandAreas) {}

    // Gets a hard-coded wall for a start position
    // Used in cases where our automatic wall generation doesn't work for whatever reason
    virtual ForgeGatewayWall getWall(BWAPI::TilePosition startTile) { return {}; }

    // Performs map-specific changes to the list of bases available on this map
    virtual void modifyBases(std::vector<Base *> &bases) {}
};
