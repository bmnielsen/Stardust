#include "Units.h"
#include "Geo.h"

namespace Units
{
#ifndef _DEBUG
    namespace
    {
#endif
        std::map<BWAPI::Unit, std::shared_ptr<Unit>> bwapiUnitToUnit;
        std::map<BWAPI::Player, std::set<std::shared_ptr<Unit>>> playerToUnits;

        std::map<BWAPI::Unit, std::unique_ptr<MyUnit>> bwapiUnitToMyUnit;

        std::map<BWAPI::UnitType, std::set<BWAPI::Unit>> myCompletedUnitsByType;
        std::map<BWAPI::UnitType, std::set<BWAPI::Unit>> myIncompleteUnitsByType;
#ifndef _DEBUG
    }
#endif

    void update()
    {
        // Update all units
        for (auto player : BWAPI::Broodwar->getPlayers())
        {
            // Ignore neutral units
            if (player->isNeutral()) continue;

            // First handle units that are gone from the last recorded position
            for (auto & bwapiUnitAndUnit : bwapiUnitToUnit)
                bwapiUnitAndUnit.second->updateLastPositionValidity();

            // Now update all the units we can see
            for (auto unit : player->getUnits())
            {
                get(unit).update(unit);

                // Additionally do some updates for our own units
                if (player == BWAPI::Broodwar->self())
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
            }
        }

        return;

        std::ostringstream debug;
        bool anyDebugUnits = false;

        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType().isWorker() && true)
            {
                anyDebugUnits = true;

                debug << "\n" << unit->getType() << " " << unit->getID() << " @ " << unit->getPosition() << "^" << BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(unit->getPosition()));
                debug << ",speed=" << (unit->getVelocityX() * unit->getVelocityX() + unit->getVelocityY() * unit->getVelocityY()) << ": ";

                debug << "command: " << unit->getLastCommand().getType() << ",frame=" << (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame());
                if (unit->getLastCommand().getTarget())
                    debug << ",target=" << unit->getLastCommand().getTarget()->getType() << " " << unit->getLastCommand().getTarget()->getID() << " @ " << unit->getLastCommand().getTarget()->getPosition() << ",dist=" << unit->getLastCommand().getTarget()->getDistance(unit) << ",sqdist=" << Geo::EdgeToEdgeSquaredDistance(unit->getType(), unit->getPosition(), unit->getLastCommand().getTarget()->getType(), unit->getLastCommand().getTarget()->getPosition());
                else if (unit->getLastCommand().getTargetPosition())
                    debug << ",targetpos " << unit->getLastCommand().getTargetPosition();

                debug << ". order: " << unit->getOrder() << ",timer=" << unit->getOrderTimer();
                if (unit->getOrderTarget())
                    debug << ",target=" << unit->getOrderTarget()->getType() << " " << unit->getOrderTarget()->getID() << " @ " << unit->getOrderTarget()->getPosition() << ",dist=" << unit->getOrderTarget()->getDistance(unit) << ",sqdist=" << Geo::EdgeToEdgeSquaredDistance(unit->getType(), unit->getPosition(), unit->getOrderTarget()->getType(), unit->getOrderTarget()->getPosition());
                else if (unit->getOrderTargetPosition())
                    debug << ",targetpos " << unit->getOrderTargetPosition();
                
                debug << ". isMoving=" << unit->isMoving();
                if (unit->isCarryingMinerals())
                    debug << ",carrying minerals";
                if (unit->isCarryingGas())
                    debug << ",carrying gas";
            }

            if (unit->getType().isBuilding() && false)
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
        auto it = bwapiUnitToUnit.find(unit);
        if (it != bwapiUnitToUnit.end())
        {
            playerToUnits[unit->getPlayer()].erase(it->second);
            bwapiUnitToUnit.erase(it);
        }

        bwapiUnitToMyUnit.erase(unit);
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
            get(unit);
    }

    MyUnit& getMine(BWAPI::Unit unit)
    {
        auto it = bwapiUnitToMyUnit.find(unit);
        if (it != bwapiUnitToMyUnit.end())
            return *it->second;

        return *bwapiUnitToMyUnit.emplace(unit, std::make_unique<MyUnit>(unit)).first->second;
    }

    Unit& get(BWAPI::Unit unit)
    {
        auto it = bwapiUnitToUnit.find(unit);
        if (it != bwapiUnitToUnit.end())
            return *it->second;

        auto newUnit = std::make_shared<Unit>(unit);
        bwapiUnitToUnit.emplace(unit, newUnit);
        playerToUnits[unit->getPlayer()].emplace(newUnit);
        return *newUnit;
    }

    std::set<std::shared_ptr<Unit>> & getForPlayer(BWAPI::Player player)
    {
        return playerToUnits[player];
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
