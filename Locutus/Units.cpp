#include "Units.h"

namespace Units
{
#ifndef _DEBUG
    namespace
    {
#endif
        std::map<BWAPI::Unit, std::unique_ptr<MyUnit>> myUnits;
        std::map<BWAPI::Unit, std::unique_ptr<EnemyUnit>> enemyUnits;

        std::map<BWAPI::UnitType, std::set<BWAPI::Unit>> myCompletedUnitsByType;
        std::map<BWAPI::UnitType, std::set<BWAPI::Unit>> myIncompleteUnitsByType;
#ifndef _DEBUG
    }
#endif

    void update()
    {
        // Update all my units
        for (auto & unit : BWAPI::Broodwar->self()->getUnits())
        {
            getMine(unit).update();

            if (unit->isCompleted())
            {
                myCompletedUnitsByType[unit->getType()].insert(unit);
                myIncompleteUnitsByType[unit->getType()].erase(unit);
            }
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
                getEnemy(unit).update(unit);
            }
        }

        return;

        std::ostringstream debug;
        bool anyDebugUnits = false;

        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType().isWorker())
            {
                anyDebugUnits = true;

                debug << "\n" << unit->getType() << " " << unit->getID() << " @ " << unit->getPosition() << "^" << BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(unit->getPosition())) << ": ";

                debug << "command: " << unit->getLastCommand().getType() << ",frame=" << (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame());
                if (unit->getLastCommand().getTarget())
                    debug << ",target=" << unit->getLastCommand().getTarget()->getType() << " " << unit->getLastCommand().getTarget()->getID() << " @ " << unit->getLastCommand().getTarget()->getPosition() << ",dist=" << unit->getLastCommand().getTarget()->getDistance(unit);
                else if (unit->getLastCommand().getTargetPosition())
                    debug << ",targetpos " << unit->getLastCommand().getTargetPosition();

                debug << ". order: " << unit->getOrder();
                if (unit->getOrderTarget())
                    debug << ",target=" << unit->getOrderTarget()->getType() << " " << unit->getOrderTarget()->getID() << " @ " << unit->getOrderTarget()->getPosition() << ",dist=" << unit->getOrderTarget()->getDistance(unit);
                else if (unit->getOrderTargetPosition())
                    debug << ",targetpos " << unit->getOrderTargetPosition();

                debug << ". isMoving=" << unit->isMoving();
            }

            if (unit->getType().isBuilding())
            {
                anyDebugUnits = true;

                debug << "\n" << unit->getType() << " " << unit->getID() << " @ " << unit->getPosition() << "^" << BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(unit->getPosition())) << ": ";

                debug << "completed=" << unit->isCompleted() << ",remainingBuildTime=" << unit->getRemainingBuildTime();
            }
        }

        if (anyDebugUnits) Log::Debug() << debug.str();
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

    MyUnit& getMine(BWAPI::Unit unit)
    {
        auto it = myUnits.find(unit);
        if (it != myUnits.end())
            return *it->second;

        return *myUnits.emplace(unit, std::make_unique<MyUnit>(unit)).first->second;
    }

    EnemyUnit& getEnemy(BWAPI::Unit unit)
    {
        auto it = enemyUnits.find(unit);
        if (it != enemyUnits.end())
            return *it->second;

        return *enemyUnits.emplace(unit, std::make_unique<EnemyUnit>(unit)).first->second;
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
