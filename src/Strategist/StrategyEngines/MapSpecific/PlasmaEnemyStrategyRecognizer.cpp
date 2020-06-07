#include "PlasmaStrategyEngine.h"

#include "Units.h"
#include "Map.h"
#include "UnitUtil.h"

std::map<PlasmaStrategyEngine::EnemyStrategy, std::string> PlasmaStrategyEngine::EnemyStrategyNames = {
        {EnemyStrategy::Unknown,   "Unknown"},
        {EnemyStrategy::GasSteal,  "GasSteal"},
        {EnemyStrategy::ProxyRush, "ProxyRush"},
        {EnemyStrategy::Normal,    "Normal"},
};

namespace
{
    bool isGasSteal()
    {
        for (const Unit &unit : Units::allEnemyOfType(BWAPI::Broodwar->enemy()->getRace().getRefinery()))
        {
            if (!unit->lastPositionValid) continue;
            if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition)) != Map::getMyMain()->getArea()) continue;

            return true;
        }

        return false;
    }

    bool isProxy()
    {
        if (BWAPI::Broodwar->getFrameCount() >= 10000) return false;

        // We expect enemies to bug out or do weird things on Plasma, so ignore their main base timings

        // Check if we have directly scouted an enemy building or ground combat unit in one of our main areas
        std::set<const BWEM::Area *> accessibleAreas;
        Map::mapSpecificOverride()->addMainBaseBuildingPlacementAreas(accessibleAreas);
        for (const auto &enemyUnit : Units::allEnemy())
        {
            if (enemyUnit->isFlying) continue;
            if (!enemyUnit->type.isBuilding() && !UnitUtil::IsCombatUnit(enemyUnit->type)) continue;
            if (enemyUnit->type.isRefinery()) continue; // Don't count gas steals
            if (enemyUnit->type.isResourceDepot()) continue; // Don't count expansions

            auto area = BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(enemyUnit->lastPosition));
            if (accessibleAreas.find(area) != accessibleAreas.end()) return true;
        }

        return false;
    }
}

PlasmaStrategyEngine::EnemyStrategy PlasmaStrategyEngine::recognizeEnemyStrategy()
{
    auto strategy = enemyStrategy;
    for (int i = 0; i < 10; i++)
    {
        switch (strategy)
        {
            case EnemyStrategy::Unknown:
                // Assume zergs won't do a proxy, and that others would do a proxy before frame 6000
                if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg ||
                    BWAPI::Broodwar->getFrameCount() > 6000)
                {
                    strategy = EnemyStrategy::Normal;
                    continue;
                }

                if (isGasSteal()) return EnemyStrategy::GasSteal;
                if (isProxy()) return EnemyStrategy::ProxyRush;

                break;
            case EnemyStrategy::GasSteal:
                if (!isGasSteal())
                {
                    strategy = EnemyStrategy::Unknown;
                    continue;
                }
                break;
            case EnemyStrategy::ProxyRush:
                if (isGasSteal()) return EnemyStrategy::GasSteal;

                if (!isProxy())
                {
                    strategy = EnemyStrategy::Normal;
                    continue;
                }

                break;
            case EnemyStrategy::Normal:
                if (isGasSteal()) return EnemyStrategy::GasSteal;

                break;
        }

        return strategy;
    }

    Log::Get() << "ERROR: Loop in strategy recognizer, ended on " << EnemyStrategyNames[strategy];
    return strategy;
}
