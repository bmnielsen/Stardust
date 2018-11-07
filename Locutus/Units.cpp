#include "Units.h"

namespace Units
{
    namespace 
    {
        std::map<BWAPI::Unit, MyUnit> myUnits;
        std::map<BWAPI::Unit, EnemyUnit> enemyUnits;
    }

    void update()
    {
        // Update all my units
        for (auto & unit : BWAPI::Broodwar->self()->getUnits())
        {
            getMine(unit).update();
        }

        // Update all enemy units
        for (auto player : BWAPI::Broodwar->getPlayers())
        {
            if (!player->isEnemy(BWAPI::Broodwar->self())) continue;

            // First handle units that are gone from the last recorded position
            for (auto & unit : enemyUnits)
                unit.second.updateLastPositionValidity();

            // Now update all the units we can see
            for (auto unit : player->getUnits())
            {
                getEnemy(unit).update(unit);
            }
        }
    }

    void onUnitDestroy(BWAPI::Unit unit)
    {
        myUnits.erase(unit);
        enemyUnits.erase(unit);
    }

    void onUnitRenegade(BWAPI::Unit unit)
    {
        // Clear from any map it is in
        onUnitDestroy(unit);

        // Maybe add to an appropriate map
        if (unit->getPlayer() == BWAPI::Broodwar->self())
            getMine(unit);
        else if (unit->getPlayer()->isEnemy(BWAPI::Broodwar->self()))
            getEnemy(unit);
    }

    const std::map<BWAPI::Unit, MyUnit> & allMine()
    {
        return myUnits;
    }

    MyUnit & getMine(BWAPI::Unit unit)
    {
        auto it = myUnits.find(unit);
        if (it != myUnits.end())
            return it->second;

        return myUnits.emplace(unit, unit).first->second;
    }

    const std::map<BWAPI::Unit, EnemyUnit> & allEnemy()
    {
        return enemyUnits;
    }

    EnemyUnit & getEnemy(BWAPI::Unit unit)
    {
        auto it = enemyUnits.find(unit);
        if (it != enemyUnits.end())
            return it->second;

        return enemyUnits.emplace(unit, unit).first->second;
    }
}
