#include "Units.h"
#include "Geo.h"
#include "Players.h"
#include "Workers.h"
#include "Map.h"
#include "BuildingPlacement.h"
#include "MyDragoon.h"
#include "MyWorker.h"

namespace Units
{
    namespace
    {
        std::set<MyUnit> myUnits;
        std::set<Unit> enemyUnits;

        std::map<int, MyUnit> unitIdToMyUnit;
        std::map<int, Unit> unitIdToEnemyUnit;

        std::map<BWAPI::UnitType, std::set<MyUnit>> myCompletedUnitsByType;
        std::map<BWAPI::UnitType, std::set<MyUnit>> myIncompleteUnitsByType;

        std::set<BWAPI::UpgradeType> upgradesInProgress;

        void unitCreated(const Unit &unit)
        {
            Map::onUnitCreated(unit);
            BuildingPlacement::onUnitCreate(unit);

            if (unit->player == BWAPI::Broodwar->self())
            {
                Log::Get() << "Unit created: " << *unit;
            }

            if (unit->player == BWAPI::Broodwar->enemy())
            {
                Log::Get() << "Enemy discovered: " << *unit;
            }
        }

        void unitDestroyed(const Unit &unit)
        {
            if (unit->lastPositionValid)
            {
                Players::grid(unit->player).unitDestroyed(unit->type, unit->lastPosition, unit->completed);
#if DEBUG_GRID_UPDATES
                CherryVis::log(unit->id) << "Grid::unitDestroyed " << unit->lastPosition;
#endif
            }

            Map::onUnitDestroy(unit);
            Workers::onUnitDestroy(unit);
            BuildingPlacement::onUnitDestroy(unit);

            unit->bwapiUnit = nullptr; // Signals to all holding a copy of the pointer that this unit is dead
        }

        void myUnitDestroyed(const MyUnit &unit)
        {
            Log::Get() << "Unit lost: " << *unit;

            unitDestroyed(unit);

            myCompletedUnitsByType[unit->type].erase(unit);
            myIncompleteUnitsByType[unit->type].erase(unit);

            myUnits.erase(unit);
            unitIdToMyUnit.erase(unit->id);
        }

        void enemyUnitDestroyed(const Unit &unit)
        {
            Log::Get() << "Enemy destroyed: " << *unit;

            unitDestroyed(unit);

            enemyUnits.erase(unit);
            unitIdToEnemyUnit.erase(unit->id);
        }
    }

    void initialize()
    {
        myUnits.clear();
        enemyUnits.clear();
        unitIdToMyUnit.clear();
        unitIdToEnemyUnit.clear();
        myCompletedUnitsByType.clear();
        myIncompleteUnitsByType.clear();
        upgradesInProgress.clear();
    }

    void update()
    {
        upgradesInProgress.clear();

        // Update our units
        // We always have vision of our own units, so we don't have to handle units in fog
        for (auto bwapiUnit : BWAPI::Broodwar->self()->getUnits())
        {
            // If we just mind controlled an enemy unit, consider the enemy unit destroyed
            auto enemyIt = unitIdToEnemyUnit.find(bwapiUnit->getID());
            if (enemyIt != unitIdToEnemyUnit.end())
            {
                enemyUnitDestroyed(enemyIt->second);
            }

            // Create or update
            MyUnit unit;
            auto it = unitIdToMyUnit.find(bwapiUnit->getID());
            if (it == unitIdToMyUnit.end())
            {
                if (bwapiUnit->getType().isWorker())
                {
                    unit = std::make_shared<MyWorker>(bwapiUnit);
                }
                else if (bwapiUnit->getType() == BWAPI::UnitTypes::Protoss_Dragoon)
                {
                    unit = std::make_shared<MyDragoon>(bwapiUnit);
                }
                else
                {
                    unit = std::make_shared<MyUnitImpl>(bwapiUnit);
                }
                myUnits.insert(unit);
                unitIdToMyUnit.emplace(unit->id, unit);

                unitCreated(unit);
            }
            else
            {
                unit = it->second;
                unit->update(bwapiUnit);
            }

            if (unit->completed)
            {
                myCompletedUnitsByType[unit->type].insert(unit);
                myIncompleteUnitsByType[unit->type].erase(unit);
            }
            else
                myIncompleteUnitsByType[unit->type].insert(unit);

            if (bwapiUnit->isUpgrading())
            {
                upgradesInProgress.insert(bwapiUnit->getUpgrade());
            }
        }

        // Update visible enemy units
        for (auto bwapiUnit : BWAPI::Broodwar->enemy()->getUnits())
        {
            // If the enemy just mind controlled one of our units, consider our unit destroyed
            auto myIt = unitIdToMyUnit.find(bwapiUnit->getID());
            if (myIt != unitIdToMyUnit.end())
            {
                myUnitDestroyed(myIt->second);
            }

            // Create or update
            Unit unit;
            auto it = unitIdToEnemyUnit.find(bwapiUnit->getID());
            if (it == unitIdToEnemyUnit.end())
            {
                unit = std::make_shared<UnitImpl>(bwapiUnit);
                enemyUnits.insert(unit);
                unitIdToEnemyUnit.emplace(unit->id, unit);

                unitCreated(unit);
            }
            else
            {
                unit = it->second;
                unit->update(bwapiUnit);
            }
        }

        // Update enemy units in the fog
        std::vector<Unit> destroyedEnemyUnits;
        for (auto &unit : enemyUnits)
        {
            if (unit->lastSeen == BWAPI::Broodwar->getFrameCount()) continue;

            unit->updateUnitInFog();

            // If a building that can't be lifted has disappeared from its last position, treat it as destroyed
            if (!unit->lastPositionValid && unit->type.isBuilding() && !unit->type.isFlyingBuilding())
            {
                destroyedEnemyUnits.push_back(unit);
                continue;
            }
        }
        for (auto &unit : destroyedEnemyUnits)
        {
            enemyUnitDestroyed(unit);
        }

        // Output debug info for our own units
        for (auto &unit : myUnits)
        {
            bool output = false;
#if DEBUG_PROBE_STATUS
            output = output || unit->type == BWAPI::UnitTypes::Protoss_Probe;
#endif
#if DEBUG_ZEALOT_STATUS
            output = output || unit->type == BWAPI::UnitTypes::Protoss_Zealot;
#endif
#if DEBUG_DRAGOON_STATUS
            output = output || unit->type == BWAPI::UnitTypes::Protoss_Dragoon;
#endif
#if DEBUG_SHUTTLE_STATUS
            output = output || unit->type == BWAPI::UnitTypes::Protoss_Shuttle;
#endif

            if (!output) continue;

            std::ostringstream debug;

            // First line is command
            debug << "cmd=" << unit->bwapiUnit->getLastCommand().getType() << ";f="
                  << (BWAPI::Broodwar->getFrameCount() - unit->bwapiUnit->getLastCommandFrame());
            if (unit->bwapiUnit->getLastCommand().getTarget())
            {
                debug << ";tgt=" << unit->bwapiUnit->getLastCommand().getTarget()->getType()
                      << "#" << unit->bwapiUnit->getLastCommand().getTarget()->getID()
                      << "@" << BWAPI::WalkPosition(unit->bwapiUnit->getLastCommand().getTarget()->getPosition())
                      << ";d=" << unit->bwapiUnit->getLastCommand().getTarget()->getDistance(unit->bwapiUnit);
            }
            else if (unit->bwapiUnit->getLastCommand().getTargetPosition())
            {
                debug << ";tgt=" << BWAPI::WalkPosition(unit->bwapiUnit->getLastCommand().getTargetPosition());
            }

            // Next line is order
            debug << "\nord=" << unit->bwapiUnit->getOrder() << ";t=" << unit->bwapiUnit->getOrderTimer();
            if (unit->bwapiUnit->getOrderTarget())
            {
                debug << ";tgt=" << unit->bwapiUnit->getOrderTarget()->getType()
                      << "#" << unit->bwapiUnit->getOrderTarget()->getID()
                      << "@" << BWAPI::WalkPosition(unit->bwapiUnit->getOrderTarget()->getPosition())
                      << ";d=" << unit->bwapiUnit->getOrderTarget()->getDistance(unit->bwapiUnit);
            }
            else if (unit->bwapiUnit->getOrderTargetPosition())
            {
                debug << ";tgt=" << BWAPI::WalkPosition(unit->bwapiUnit->getOrderTargetPosition());
            }

            // Last line is movement data and other unit-specific stuff
            debug << "\n";
            if (unit->type.topSpeed() > 0.001)
            {
                auto speed = sqrt(unit->bwapiUnit->getVelocityX() * unit->bwapiUnit->getVelocityX()
                                  + unit->bwapiUnit->getVelocityY() * unit->bwapiUnit->getVelocityY());
                debug << "spd=" << ((int) (100.0 * speed / unit->type.topSpeed()));
            }

            debug << ";mvng=" << unit->bwapiUnit->isMoving() << ";rdy=" << unit->isReady()
                  << ";cdn=" << (unit->cooldownUntil - BWAPI::Broodwar->getFrameCount());

            if (unit->type == BWAPI::UnitTypes::Protoss_Dragoon)
            {
                auto myDragoon = std::dynamic_pointer_cast<MyDragoon>(unit);
                debug << ";atckf=" << (BWAPI::Broodwar->getFrameCount() - myDragoon->getLastAttackStartedAt());
            }

            CherryVis::log(unit->id) << debug.str();
        }
    }

    void issueOrders()
    {
        for (auto &unit : myUnits)
        {
            unit->issueMoveOrders();
        }
    }

    void onUnitDestroy(BWAPI::Unit unit)
    {
        auto it = unitIdToMyUnit.find(unit->getID());
        if (it != unitIdToMyUnit.end())
        {
            myUnitDestroyed(it->second);
        }

        auto enemyIt = unitIdToEnemyUnit.find(unit->getID());
        if (enemyIt != unitIdToEnemyUnit.end())
        {
            enemyUnitDestroyed(enemyIt->second);
        }
    }

    void onBulletCreate(BWAPI::Bullet bullet)
    {
        if (!bullet->getSource() || !bullet->getTarget()) return;

        // If this bullet is a ranged bullet that deals damage after a delay, track it on the unit it is moving towards
        if (bullet->getType() == BWAPI::BulletTypes::Gemini_Missiles ||         // Wraith
            bullet->getType() == BWAPI::BulletTypes::Fragmentation_Grenade ||   // Vulture
            bullet->getType() == BWAPI::BulletTypes::Longbolt_Missile ||        // Missile turret
            bullet->getType() == BWAPI::BulletTypes::ATS_ATA_Laser_Battery ||   // Battlecruiser
            bullet->getType() == BWAPI::BulletTypes::Burst_Lasers ||            // Wraith
            bullet->getType() == BWAPI::BulletTypes::Anti_Matter_Missile ||     // Scout
            bullet->getType() == BWAPI::BulletTypes::Pulse_Cannon ||            // Interceptor
            bullet->getType() == BWAPI::BulletTypes::Yamato_Gun ||              // Battlecruiser
            bullet->getType() == BWAPI::BulletTypes::Phase_Disruptor ||         // Dragoon
            bullet->getType() == BWAPI::BulletTypes::STA_STS_Cannon_Overlay ||  // Photon cannon?
            bullet->getType() == BWAPI::BulletTypes::Glave_Wurm ||              // Mutalisk
            bullet->getType() == BWAPI::BulletTypes::Seeker_Spores ||           // Spore colony
            bullet->getType() == BWAPI::BulletTypes::Halo_Rockets ||            // Valkyrie
            bullet->getType() == BWAPI::BulletTypes::Subterranean_Spines)       // Lurker
        {
            auto target = get(bullet->getTarget());
            auto source = get(bullet->getSource());
            if (target) target->addUpcomingAttack(source, bullet);
        }
    }

    Unit get(BWAPI::Unit unit)
    {
        if (!unit) return nullptr;

        auto it = unitIdToMyUnit.find(unit->getID());
        if (it != unitIdToMyUnit.end())
        {
            return it->second;
        }

        auto enemyIt = unitIdToEnemyUnit.find(unit->getID());
        if (enemyIt != unitIdToEnemyUnit.end())
        {
            return it->second;
        }

        return nullptr;
    }

    MyUnit mine(BWAPI::Unit unit)
    {
        auto it = unitIdToMyUnit.find(unit->getID());
        if (it != unitIdToMyUnit.end())
        {
            return it->second;
        }

        return nullptr;
    }

    std::set<MyUnit> &allMine()
    {
        return myUnits;
    }

    std::set<Unit> &allEnemy()
    {
        return enemyUnits;
    }

    void mine(std::set<MyUnit> &units,
              const std::function<bool(const MyUnit &)> &predicate)
    {
        for (auto &unit : myUnits)
        {
            if (predicate && !predicate(unit)) continue;
            units.insert(unit);
        }
    }

    void enemy(std::set<Unit> &units,
               const std::function<bool(const Unit &)> &predicate)
    {
        for (auto &unit : enemyUnits)
        {
            if (predicate && !predicate(unit)) continue;
            units.insert(unit);
        }
    }

    void enemyInRadius(std::set<Unit> &units,
                       BWAPI::Position position,
                       int radius,
                       const std::function<bool(const Unit &)> &predicate)
    {
        for (auto &unit : enemyUnits)
        {
            if (predicate && !predicate(unit)) continue;
            if (unit->lastPositionValid && unit->lastPosition.getApproxDistance(position) <= radius)
                units.insert(unit);
        }
    }

    void enemyInArea(std::set<Unit> &units,
                     const BWEM::Area *area,
                     const std::function<bool(const Unit &)> &predicate)
    {
        for (auto &unit : enemyUnits)
        {
            if (predicate && !predicate(unit)) continue;
            if (unit->lastPositionValid && BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition)) == area)
                units.insert(unit);
        }
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

    std::map<BWAPI::UnitType, int> countIncompleteByType()
    {
        std::map<BWAPI::UnitType, int> result;
        for (auto &typeAndUnits : myIncompleteUnitsByType)
        {
            result[typeAndUnits.first] = typeAndUnits.second.size();
        }
        return result;
    }

    bool isBeingUpgraded(BWAPI::UpgradeType type)
    {
        return upgradesInProgress.find(type) != upgradesInProgress.end();
    }
}
