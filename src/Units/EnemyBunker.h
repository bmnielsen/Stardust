#pragma once

#include "Common.h"
#include "Unit.h"

class EnemyBunker : public UnitImpl
{
public:
    // How many marines we think are loaded into this bunker
    int loadedMarines;

    // Whether one of our units is in the attack range of the bunker
    bool myUnitInRange;

    // The frame when myUnitInRange last changed
    int frameMyUnitInRangeLastChanged;

    explicit EnemyBunker(BWAPI::Unit unit);

    void update(BWAPI::Unit unit) override;

    void updateUnitInFog() override;

private:
    void updateMyUnitInRange();
};
