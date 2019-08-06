#include "AttackBaseSquad.h"

#include "Units.h"
#include "UnitUtil.h"
#include "Players.h"

void AttackBaseSquad::execute(UnitCluster & cluster)
{
    // Look for enemies near this cluster
    std::set<std::shared_ptr<Unit>> enemyUnits;
    Units::getInRadius(enemyUnits, BWAPI::Broodwar->enemy(), cluster.center, 480);

    // TODO: Run combat simulation

    cluster.execute(enemyUnits, targetPosition);
}