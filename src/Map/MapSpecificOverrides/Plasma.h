#pragma once

#include "MapSpecificOverride.h"

class Plasma : public MapSpecificOverride
{
public:
    bool hasMineralWalking() override { return true; }

    bool hasAttackClearableChokes() override { return true; }

    void initializeChokes(std::map<const BWEM::ChokePoint *, Choke *> &chokes) override;

    bool canUseBwemPath(BWAPI::UnitType unitType) override
    {
        // On Plasma, BWEM doesn't mark the mineral walking chokes as blocked
        // So we can use BWEM pathing for workers but nothing else
        return unitType.isWorker();
    }

    void onUnitDestroy(BWAPI::Unit unit) override;

    bool clusterMove(UnitCluster &cluster, BWAPI::Position targetPosition) override;

    void modifyMainBaseBuildingPlacementAreas(std::set<const BWEM::Area *> &areas) override;

    std::unique_ptr<StrategyEngine> createStrategyEngine() override;

    bool allowDiagonalPathingThrough(int x, int y) override
    {
        // On Plasma we allow diagonal pathing through all of the narrow ramps
        return
                (x == 13 && y == 26) || (x == 14 && y == 27) ||     // top-left main
                (x == 25 && y == 119) || (x == 26 && y == 118) ||   // bottom-left main
                (x == 81 && y == 75) || (x == 82 && y == 74) ||     // right main
                (x == 59 && y == 35) || (x == 60 && y == 36) ||     // top-right expo
                (x == 31 && y == 72) || (x == 32 && y == 71) ||     // left expo
                (x == 55 && y == 91) || (x == 56 && y == 92);       // bottom-right expo
    }

private:
    std::map<Choke *, std::set<BWAPI::Unit>> chokeToBlockingEggs;
    std::vector<const BWEM::Area *> accessibleAreas;
};
