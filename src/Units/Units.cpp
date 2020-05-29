#include "Units.h"
#include "Geo.h"
#include "Players.h"
#include "Workers.h"
#include "Map.h"
#include "BuildingPlacement.h"
#include "MyDragoon.h"
#include "MyWorker.h"
#include "UnitUtil.h"
#include "Opponent.h"

// These defines configure a per-frame summary of various unit type's orders, commands, etc.
#if INSTRUMENTATION_ENABLED_VERBOSE
#define DEBUG_PROBE_STATUS false
#define DEBUG_ZEALOT_STATUS true
#define DEBUG_DRAGOON_STATUS true
#define DEBUG_SHUTTLE_STATUS false
#define DEBUG_OBSERVER_STATUS false
#define DEBUG_ENEMY_STATUS false
#endif

#if INSTRUMENTATION_ENABLED
#define DEBUG_ENEMY_TIMINGS true
#endif

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

        std::map<BWAPI::UnitType, std::set<Unit>> enemyUnitsByType;
        std::map<BWAPI::UnitType, std::vector<std::pair<int, int>>> enemyUnitTimings;

        std::set<BWAPI::UpgradeType> upgradesInProgress;

        void unitCreated(const Unit &unit)
        {
            Map::onUnitCreated(unit);
            BuildingPlacement::onUnitCreate(unit);

            if (unit->player == BWAPI::Broodwar->self())
            {
                Log::Get() << "Unit created: " << *unit;
            }

            if (unit->player == BWAPI::Broodwar->enemy() && unit->type.isBuilding())
            {
                Log::Get() << "Enemy discovered: " << *unit;
            }
        }

        void unitDestroyed(const Unit &unit)
        {
            if (unit->lastPositionValid && !unit->beingManufacturedOrCarried)
            {
                Players::grid(unit->player).unitDestroyed(unit->type, unit->lastPosition, unit->completed, unit->burrowed);
#if DEBUG_GRID_UPDATES
                CherryVis::log(unit->id) << "Grid::unitDestroyed " << unit->lastPosition;
                Log::Debug() << *unit << ": Grid::unitDestroyed " << unit->lastPosition;
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
            if (unit->type.isBuilding())
            {
                Log::Get() << "Enemy destroyed: " << *unit;
            }

            unitDestroyed(unit);

            enemyUnits.erase(unit);
            unitIdToEnemyUnit.erase(unit->id);
            enemyUnitsByType[unit->type].erase(unit);
        }

        void trackEnemyUnitTimings(const Unit &unit, bool includeMorphs)
        {
            // Don't track eggs or larva
            if (unit->type == BWAPI::UnitTypes::Zerg_Larva ||
                unit->type == BWAPI::UnitTypes::Zerg_Egg)
            {
                return;
            }

            // If this is the opponent's initial depot, update the element we already added
            if (unit->type.isResourceDepot() && !enemyUnitTimings[unit->type].empty() && Map::getEnemyStartingMain()
                && unit->getTilePosition() == Map::getEnemyStartingMain()->getTilePosition())
            {
                enemyUnitTimings[unit->type].begin()->second = BWAPI::Broodwar->getFrameCount();
                return;
            }

            // TODO: Do we need to handle the initial units?

            // Estimate the frame the unit completed
            int completionFrame;
            if (unit->type.isBuilding())
            {
                completionFrame = unit->completed ? BWAPI::Broodwar->getFrameCount() : unit->estimatedCompletionFrame;
            }
            else
            {
                // Assume the unit came from the enemy main
                // TODO: Do we need to make this more accurate and consider other known bases?

                int framesToEnemyMain = INT_MAX;
                auto enemyMain = Map::getEnemyMain();
                if (!enemyMain) enemyMain = Map::getEnemyStartingMain();
                if (!enemyMain)
                {
                    // Default to the closest unscouted starting position
                    for (auto base : Map::unscoutedStartingLocations())
                    {
                        int frames = PathFinding::ExpectedTravelTime(unit->lastPosition,
                                                                     base->getPosition(),
                                                                     unit->type,
                                                                     PathFinding::PathFindingOptions::UseNearestBWEMArea);
                        if (frames < framesToEnemyMain)
                        {
                            framesToEnemyMain = frames;
                            enemyMain = base;
                        }
                    }
                }

                if (enemyMain && unit->type.topSpeed() > 0.001)
                {
                    if (framesToEnemyMain == INT_MAX)
                    {
                        framesToEnemyMain = PathFinding::ExpectedTravelTime(unit->lastPosition,
                                                                            enemyMain->getPosition(),
                                                                            unit->type,
                                                                            PathFinding::PathFindingOptions::UseNearestBWEMArea);
                    }
                    completionFrame = BWAPI::Broodwar->getFrameCount() - framesToEnemyMain;
                }
                else
                {
                    completionFrame = BWAPI::Broodwar->getFrameCount();
                }
            }

            int startFrame = completionFrame - UnitUtil::BuildTime(unit->type);
            enemyUnitTimings[unit->type].emplace_back(std::make_pair(startFrame, BWAPI::Broodwar->getFrameCount()));

            if (enemyUnitTimings[unit->type].size() == 1)
            {
                Log::Get() << "First enemy of type discovered: " << *unit;
            }

            if (unit->type.isBuilding() && includeMorphs)
            {
                auto morphsFrom = UnitUtil::MorphsFrom(unit->type);
                if (morphsFrom.first != BWAPI::UnitTypes::Zerg_Drone)
                {
                    enemyUnitTimings[morphsFrom.first].emplace_back(std::make_pair(startFrame - UnitUtil::BuildTime(morphsFrom.first),
                                                                                   BWAPI::Broodwar->getFrameCount()));
                }
            }
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
        enemyUnitsByType.clear();
        enemyUnitTimings.clear();
        upgradesInProgress.clear();

        // Add a placeholder for the enemy depot to the timings
        if (Opponent::isUnknownRace())
        {
            // For a random opponent, we don't know what type of depot they have, so just add one of each
            // The matchup-specific strategy engines will never query for offrace types anyway, so the extras are meaningless
            enemyUnitTimings[BWAPI::UnitTypes::Protoss_Nexus].emplace_back(std::make_pair(0, INT_MAX));
            enemyUnitTimings[BWAPI::UnitTypes::Terran_Command_Center].emplace_back(std::make_pair(0, INT_MAX));
            enemyUnitTimings[BWAPI::UnitTypes::Zerg_Hatchery].emplace_back(std::make_pair(0, INT_MAX));
        }
        else
        {
            enemyUnitTimings[BWAPI::Broodwar->enemy()->getRace().getResourceDepot()].emplace_back(std::make_pair(0, INT_MAX));
        }
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
                    unit->created();
                }
                else if (bwapiUnit->getType() == BWAPI::UnitTypes::Protoss_Dragoon)
                {
                    unit = std::make_shared<MyDragoon>(bwapiUnit);
                    unit->created();
                }
                else
                {
                    unit = std::make_shared<MyUnitImpl>(bwapiUnit);
                    unit->created();
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
            if (!bwapiUnit->isVisible()) continue;

#if DEBUG_ENEMY_STATUS
            CherryVis::log(bwapiUnit->getID()) << "VIS"
                                               << ";cmpl=" << bwapiUnit->isCompleted()
                                               << ";iSA=" << bwapiUnit->isStartingAttack()
                                               << ";iA=" << bwapiUnit->isAttacking()
                                               << ";iAF=" << bwapiUnit->isAttackFrame()
                                               << ";cdwn=" << bwapiUnit->getGroundWeaponCooldown();
#endif

            // If the enemy just mind controlled one of our units, consider our unit destroyed
            auto myIt = unitIdToMyUnit.find(bwapiUnit->getID());
            if (myIt != unitIdToMyUnit.end())
            {
                myUnitDestroyed(myIt->second);
            }

            // Create or update
            auto it = unitIdToEnemyUnit.find(bwapiUnit->getID());
            if (it != unitIdToEnemyUnit.end())
            {
                // If the type is still the same, update and continue
                if (it->second->type == bwapiUnit->getType())
                {
                    it->second->update(bwapiUnit);
                    continue;
                }

                // The unit has morphed - for simplicity consider the old one as destroyed and the new one created
                enemyUnitDestroyed(it->second);
            }

            auto unit = std::make_shared<UnitImpl>(bwapiUnit);
            unit->created();
            enemyUnits.insert(unit);
            unitIdToEnemyUnit.emplace(unit->id, unit);

            unitCreated(unit);

            enemyUnitsByType[unit->type].insert(unit);
            trackEnemyUnitTimings(unit, it == unitIdToEnemyUnit.end());
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

#if DEBUG_ENEMY_STATUS
            CherryVis::log(unit->id) << "FOG;cmpl=" << unit->completed << ";lpv=" << unit->lastPositionValid;
#endif
        }
        for (auto &unit : destroyedEnemyUnits)
        {
            enemyUnitDestroyed(unit);
        }

        // Occasionally check for any inconsistencies in the enemy unit collections
        if (BWAPI::Broodwar->getFrameCount() % 48 == 0)
        {
            for (auto it = enemyUnits.begin(); it != enemyUnits.end();)
            {
                auto &unit = *it;
                if (!unit->exists())
                {
                    Log::Get() << "ERROR: " << *unit << " in enemyUnits does not exist!";
                    it = enemyUnits.erase(it);
                }
                else if (unit->player != BWAPI::Broodwar->enemy())
                {
                    Log::Get() << "ERROR: " << *unit << " in enemyUnits not owned by enemy!";
                    it = enemyUnits.erase(it);
                }
                else if (unit->bwapiUnit->isVisible() && unit->bwapiUnit->getPlayer() != BWAPI::Broodwar->enemy())
                {
                    Log::Get() << "ERROR: " << *unit << " in enemyUnits not owned by enemy (bwapiUnit)!";
                    it = enemyUnits.erase(it);
                }
                else
                {
                    it++;
                }
            }

            for (auto it = unitIdToEnemyUnit.begin(); it != unitIdToEnemyUnit.end();)
            {
                auto &unit = it->second;
                if (!unit->exists())
                {
                    Log::Get() << "ERROR: " << *unit << " in unitIdToEnemyUnit does not exist!";
                    it = unitIdToEnemyUnit.erase(it);
                }
                else if (unit->player != BWAPI::Broodwar->enemy())
                {
                    Log::Get() << "ERROR: " << *unit << " in unitIdToEnemyUnit not owned by enemy!";
                    it = unitIdToEnemyUnit.erase(it);
                }
                else if (unit->bwapiUnit->isVisible() && unit->bwapiUnit->getPlayer() != BWAPI::Broodwar->enemy())
                {
                    Log::Get() << "ERROR: " << *unit << " in unitIdToEnemyUnit not owned by enemy (bwapiUnit)!";
                    it = unitIdToEnemyUnit.erase(it);
                }
                else
                {
                    it++;
                }
            }

            for (auto &typeAndEnemyUnits : enemyUnitsByType)
            {
                for (auto it = typeAndEnemyUnits.second.begin(); it != typeAndEnemyUnits.second.end();)
                {
                    auto &unit = *it;
                    if (!unit->exists())
                    {
                        Log::Get() << "ERROR: " << *unit << " in enemyUnitsByType does not exist!";
                        it = typeAndEnemyUnits.second.erase(it);
                    }
                    else if (unit->player != BWAPI::Broodwar->enemy())
                    {
                        Log::Get() << "ERROR: " << *unit << " in enemyUnitsByType not owned by enemy!";
                        it = typeAndEnemyUnits.second.erase(it);
                    }
                    else if (unit->bwapiUnit->isVisible() && unit->bwapiUnit->getPlayer() != BWAPI::Broodwar->enemy())
                    {
                        Log::Get() << "ERROR: " << *unit << " in enemyUnitsByType not owned by enemy (bwapiUnit)!";
                        it = typeAndEnemyUnits.second.erase(it);
                    }
                    else
                    {
                        it++;
                    }
                }
            }
        }

        // Output debug info for our own units
        for (auto &unit : myUnits)
        {
            if (!unit->completed) continue;

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
#if DEBUG_OBSERVER_STATUS
            output = output || unit->type == BWAPI::UnitTypes::Protoss_Observer;
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
                  << ";stk=" << unit->bwapiUnit->isStuck()
                  << ";lstmv=" << (unit->getLastMoveFrame() - BWAPI::Broodwar->getFrameCount())
                  << ";cdn=" << (unit->cooldownUntil - BWAPI::Broodwar->getFrameCount());

            if (unit->getUnstickUntil() >= BWAPI::Broodwar->getFrameCount())
            {
                debug << ";unstck=" << (unit->getUnstickUntil() - BWAPI::Broodwar->getFrameCount());
            }

            if (unit->type == BWAPI::UnitTypes::Protoss_Dragoon)
            {
                debug << "\n";

                auto myDragoon = std::dynamic_pointer_cast<MyDragoon>(unit);
                debug << "lstatk=" << myDragoon->getLastAttackStartedAt();
                debug << ";nxtatk=" << myDragoon->getNextAttackPredictedAt();
                debug << ";stkf=" << myDragoon->getPotentiallyStuckSince();
            }

            CherryVis::log(unit->id) << debug.str();
        }

#if DEBUG_ENEMY_TIMINGS
        std::vector<std::string> values;
        for (auto &typeAndTimings : enemyUnitTimings)
        {
            if (typeAndTimings.second.empty()) continue;

            std::ostringstream timings;
            bool first = true;
            for (auto timing : typeAndTimings.second)
            {
                if (first)
                {
                    timings << typeAndTimings.first << ": ";
                    first = false;
                }
                else
                {
                    timings << ", ";
                }
                timings << timing.first << ":" << timing.second;
            }

            values.emplace_back(timings.str());
        }
        CherryVis::setBoardListValue("enemyUnitTimings", values);
#endif
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
            return enemyIt->second;
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

    MyUnit myBuildingAt(BWAPI::TilePosition tile)
    {
        for (auto unit : myUnits)
        {
            if (!unit->type.isBuilding()) continue;
            if (unit->getTilePosition() == tile) return unit;
        }

        return nullptr;
    }

    std::set<MyUnit> &allMine()
    {
        return myUnits;
    }

    std::set<MyUnit> &allMineCompletedOfType(BWAPI::UnitType type)
    {
        return myCompletedUnitsByType[type];
    }

    std::set<MyUnit> &allMineIncompleteOfType(BWAPI::UnitType type)
    {
        return myIncompleteUnitsByType[type];
    }

    std::set<Unit> &allEnemy()
    {
        return enemyUnits;
    }

    std::set<Unit> &allEnemyOfType(BWAPI::UnitType type)
    {
        return enemyUnitsByType[type];
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

    int countEnemy(BWAPI::UnitType type)
    {
        return enemyUnitsByType[type].size();
    }

    std::vector<std::pair<int, int>> &getEnemyUnitTimings(BWAPI::UnitType type)
    {
        return enemyUnitTimings[type];
    }

    bool hasEnemyBuilt(BWAPI::UnitType type)
    {
        auto it = enemyUnitTimings.find(type);
        return it != enemyUnitTimings.end() && !it->second.empty();
    }

    bool isBeingUpgraded(BWAPI::UpgradeType type)
    {
        return upgradesInProgress.find(type) != upgradesInProgress.end();
    }
}
