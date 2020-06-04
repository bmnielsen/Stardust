#pragma once

#include "MapSpecificOverride.h"

class Plasma : public MapSpecificOverride
{
public:
    bool hasMineralWalking() override { return true; }

    void initializeChokes(std::map<const BWEM::ChokePoint *, Choke *> &chokes) override;

    bool canUseBwemPath(BWAPI::UnitType unitType) override
    {
        // On Plasma, BWEM doesn't mark the mineral walking chokes as blocked
        // So we can use BWEM pathing for workers but nothing else
        return unitType.isWorker();
    }

    void onUnitDestroy(BWAPI::Unit unit) override;

    bool clusterMove(UnitCluster &cluster, BWAPI::Position targetPosition) override;

private:
    std::map<Choke *, std::set<BWAPI::Unit>> chokeToBlockingEggs;
};
