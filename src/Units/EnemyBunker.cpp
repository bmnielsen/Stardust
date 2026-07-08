#include "EnemyBunker.h"

EnemyBunker::EnemyBunker(BWAPI::Unit unit) : UnitImpl(unit), loadedMarines(0), myUnitInRange(false), frameMyUnitInRangeLastChanged(-1)
{
}

void EnemyBunker::update(BWAPI::Unit unit)
{
    UnitImpl::update(unit);

    updateMyUnitInRange();
}

void EnemyBunker::updateUnitInFog()
{
    UnitImpl::updateUnitInFog();

    updateMyUnitInRange();
}

void EnemyBunker::updateMyUnitInRange()
{

}
