#include "Units.h"

namespace Units
{
    namespace 
    {
        std::map<BWAPI::Unit, std::shared_ptr<MyUnit>> myUnits;
        std::map<BWAPI::Unit, std::shared_ptr<EnemyUnit>> enemyUnits;

        std::map<BWAPI::UnitType, std::set<BWAPI::Unit>> myCompletedUnitsByType;
        std::map<BWAPI::UnitType, std::set<BWAPI::Unit>> myIncompleteUnitsByType;
    }

    void update()
    {
        // Update all my units
        for (auto & unit : BWAPI::Broodwar->self()->getUnits())
        {
            getMine(unit)->update();

            if (unit->isCompleted())
                myCompletedUnitsByType[unit->getType()].insert(unit);
            else
                myIncompleteUnitsByType[unit->getType()].insert(unit);
        }

        // Update all enemy units
        for (auto player : BWAPI::Broodwar->getPlayers())
        {
            if (!player->isEnemy(BWAPI::Broodwar->self())) continue;

            // First handle units that are gone from the last recorded position
            for (auto & unit : enemyUnits)
                unit.second->updateLastPositionValidity();

            // Now update all the units we can see
            for (auto unit : player->getUnits())
            {
                getEnemy(unit)->update(unit);
            }
        }
    }

    void onUnitDestroy(BWAPI::Unit unit)
    {
        myUnits.erase(unit);
        enemyUnits.erase(unit);

        myCompletedUnitsByType[unit->getType()].erase(unit);
        myIncompleteUnitsByType[unit->getType()].erase(unit);
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

    const std::map<BWAPI::Unit, std::shared_ptr<MyUnit>> & allMine()
    {
        return myUnits;
    }

    std::shared_ptr<MyUnit> getMine(BWAPI::Unit unit)
    {
        auto it = myUnits.find(unit);
        if (it != myUnits.end())
            return it->second;

        return myUnits.emplace(unit, std::make_shared<MyUnit>(unit)).first->second;
    }

    const std::map<BWAPI::Unit, std::shared_ptr<EnemyUnit>> & allEnemy()
    {
        return enemyUnits;
    }

    std::shared_ptr<EnemyUnit> getEnemy(BWAPI::Unit unit)
    {
        auto it = enemyUnits.find(unit);
        if (it != enemyUnits.end())
            return it->second;

        return enemyUnits.emplace(unit, std::make_shared<EnemyUnit>(unit)).first->second;
    }

    int countAll(BWAPI::UnitType type)
    {
        return countCompleted(type) + countIncomplete(type);
    }

    int countCompleted(BWAPI::UnitType type)
    {
        return myCompletedUnitsByType[type].size();
    }

    int countIncomplete(BWAPI::UnitType type)
    {
        return myIncompleteUnitsByType[type].size();
    }
}
