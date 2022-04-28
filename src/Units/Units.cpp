#include "Units.h"
#include "Geo.h"
#include "Players.h"
#include "Workers.h"
#include "Map.h"
#include "NoGoAreas.h"
#include "BuildingPlacement.h"
#include "MyCannon.h"
#include "MyCarrier.h"
#include "MyDragoon.h"
#include "MyWorker.h"
#include "UnitUtil.h"
#include "Opponent.h"

#include "DebugFlag_GridUpdates.h"

// These defines configure a per-frame summary of various unit type's orders, commands, etc.
#if INSTRUMENTATION_ENABLED_VERBOSE
#define DEBUG_PROBE_STATUS false
#define DEBUG_ZEALOT_STATUS false
#define DEBUG_DRAGOON_STATUS false
#define DEBUG_DT_STATUS false
#define DEBUG_CARRIER_STATUS false
#define DEBUG_SHUTTLE_STATUS false
#define DEBUG_OBSERVER_STATUS false
#define DEBUG_ARBITER_STATUS false
#define DEBUG_PRODUCINGBUILDING_STATUS false
#define DEBUG_ENEMY_STATUS false
#define DEBUG_ENEMY_TIMINGS true
#define DEBUG_PREDICTED_POSITIONS false
#endif

namespace Units
{
    namespace
    {
        std::unordered_set<MyUnit> myUnits;
        std::unordered_set<Unit> enemyUnits;

        std::unordered_map<int, MyUnit> unitIdToMyUnit;
        std::unordered_map<int, Unit> unitIdToEnemyUnit;

        std::map<BWAPI::UnitType, std::unordered_set<MyUnit>> myCompletedUnitsByType;
        std::map<BWAPI::UnitType, std::unordered_set<MyUnit>> myIncompleteUnitsByType;

        std::map<BWAPI::UnitType, std::unordered_set<Unit>> enemyUnitsByType;
        std::map<BWAPI::UnitType, std::vector<std::pair<int, int>>> enemyUnitTimings;

        std::unordered_map<Unit, Base *> enemyUnitsToBase;
        std::unordered_map<Base *, std::unordered_set<Unit>> basesToEnemyUnits;

        std::set<BWAPI::UpgradeType> upgradesInProgress;
        std::set<BWAPI::TechType> researchInProgress;

        void trackResearch(const BWAPI::Unit bwapiUnit)
        {
            if (bwapiUnit->getPlayer() == BWAPI::Broodwar->self())
            {
                // Terran
                if (bwapiUnit->isLockedDown()) Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Lockdown);
                if (bwapiUnit->isIrradiated()) Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Irradiate);
                if (bwapiUnit->isBlind()) Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Optical_Flare);

                // Zerg
                if (bwapiUnit->isUnderDarkSwarm()) Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Dark_Swarm);
                if (bwapiUnit->isPlagued()) Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Plague);
                if (bwapiUnit->isEnsnared()) Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Ensnare);
                if (bwapiUnit->isParasited()) Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Parasite);

                return;
            }

            // Terran
            if (bwapiUnit->isStimmed())
            {
                Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Stim_Packs);
            }
            if (bwapiUnit->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
            {
                Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Spider_Mines);
            }
            if (bwapiUnit->getType() == BWAPI::UnitTypes::Spell_Scanner_Sweep)
            {
                Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Scanner_Sweep);
            }
            if (bwapiUnit->isSieged())
            {
                Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Tank_Siege_Mode);
            }
            if (bwapiUnit->isDefenseMatrixed())
            {
                Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Defensive_Matrix);
            }
            if (bwapiUnit->getType() == BWAPI::UnitTypes::Terran_Wraith && bwapiUnit->isCloaked())
            {
                Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Cloaking_Field);
            }
            if (bwapiUnit->getType() == BWAPI::UnitTypes::Terran_Ghost && bwapiUnit->isCloaked())
            {
                Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Personnel_Cloaking);
            }
            if (bwapiUnit->getType() == BWAPI::UnitTypes::Terran_Nuclear_Silo ||
                bwapiUnit->getType() == BWAPI::UnitTypes::Terran_Nuclear_Missile)
            {
                Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Nuclear_Strike);
            }

            // Zerg
            if (bwapiUnit->getType() != BWAPI::UnitTypes::Zerg_Lurker &&
                (bwapiUnit->isBurrowed() || bwapiUnit->getOrder() == BWAPI::Orders::Burrowing || bwapiUnit->getOrder() == BWAPI::Orders::Unburrowing))
            {
                Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Burrowing);
            }
            if (bwapiUnit->getType() == BWAPI::UnitTypes::Zerg_Broodling)
            {
                Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Spawn_Broodlings);
            }
            if (bwapiUnit->getType() == BWAPI::UnitTypes::Spell_Dark_Swarm)
            {
                Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Dark_Swarm);
            }
            if (bwapiUnit->getType() == BWAPI::UnitTypes::Zerg_Lurker_Egg ||
                bwapiUnit->getType() == BWAPI::UnitTypes::Zerg_Lurker)
            {
                Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Lurker_Aspect);
            }

            // Protoss
            if (bwapiUnit->getOrder() == BWAPI::Orders::CastRecall)
            {
                Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Recall);
            }
            if (bwapiUnit->getOrder() == BWAPI::Orders::CastStasisField)
            {
                Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Stasis_Field);
            }
            if (bwapiUnit->getType() == BWAPI::UnitTypes::Spell_Disruption_Web)
            {
                Players::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Disruption_Web);
            }
        }

        void unitCreated(const Unit &unit)
        {
            Map::onUnitCreated(unit);
            BuildingPlacement::onUnitCreate(unit);
            NoGoAreas::onUnitCreate(unit);

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
            if (unit->lastPositionValid && !unit->beingManufacturedOrCarried && (!unit->type.isBuilding() || !unit->isFlying))
            {
                Players::grid(unit->player).unitDestroyed(unit->type, unit->lastPosition, unit->completed, unit->burrowed, unit->immobile);
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

            // Don't track a gas steal
            if (unit->type.isRefinery() && unit->getDistance(Map::getMyMain()->getPosition()) < 300) return;

            // If this is the opponent's initial depot, update the element we already added
            if (unit->type.isResourceDepot() && !enemyUnitTimings[unit->type].empty() && Map::getEnemyStartingMain()
                && unit->getTilePosition() == Map::getEnemyStartingMain()->getTilePosition())
            {
                enemyUnitTimings[unit->type].begin()->second = currentFrame;
                return;
            }

            // TODO: Do we need to handle the initial units?

            // Estimate the frame the unit completed
            int completionFrame;
            if (unit->type.isBuilding())
            {
                completionFrame = unit->completed ? currentFrame : unit->estimatedCompletionFrame;
            }
            else
            {
                auto getClosestProducer = [&unit]()
                {
                    Unit closestProducer = nullptr;
                    int closestDist = INT_MAX;

                    auto handlePossibleProducer = [&](const Unit &producer)
                    {
                        if (!producer->exists()) return;
                        if (!producer->lastPositionValid) return;

                        int dist;
                        if (unit->isFlying)
                        {
                            dist = producer->lastPosition.getApproxDistance(unit->lastPosition);
                        }
                        else
                        {
                            dist = PathFinding::GetGroundDistance(producer->lastPosition,
                                                                  unit->lastPosition,
                                                                  unit->type,
                                                                  PathFinding::PathFindingOptions::UseNearestBWEMArea);
                        }

                        if (dist < closestDist)
                        {
                            closestDist = dist;
                            closestProducer = producer;
                        }
                    };

                    // Assume all Zerg units come from a larva (at a hatchery / lair / hive)
                    if (unit->player->getRace() == BWAPI::Races::Zerg)
                    {
                        for (const auto &producer : enemyUnits)
                        {
                            if (producer->type != BWAPI::UnitTypes::Zerg_Hatchery &&
                                producer->type != BWAPI::UnitTypes::Zerg_Lair &&
                                producer->type != BWAPI::UnitTypes::Zerg_Hive)
                            {
                                continue;
                            }

                            handlePossibleProducer(producer);
                        }
                    }
                    else
                    {
                        auto producerType = unit->type.whatBuilds().first;
                        if (!producerType.isBuilding()) producerType = producerType.whatBuilds().first;

                        for (const auto &producer : enemyUnitsByType[producerType])
                        {
                            if (producer->isFlying) continue;

                            handlePossibleProducer(producer);
                        }
                    }

                    return closestProducer;
                };

                // Try to guess how many frames the unit has moved since it was built
                int framesMoved = 0;
                if (unit->type.topSpeed() > 0.001)
                {
                    // First see if we know of anything that can produce the unit
                    auto closestProducer = getClosestProducer();
                    if (closestProducer)
                    {
                        framesMoved = PathFinding::ExpectedTravelTime(unit->lastPosition,
                                                                      closestProducer->lastPosition,
                                                                      unit->type,
                                                                      PathFinding::PathFindingOptions::UseNearestBWEMArea);

                        // Give it a bit of leeway to account for the spawn position being outside the producer
                        framesMoved = std::max(0, framesMoved - 48);
                    }
                    else
                    {
                        // We don't know the producer, so assume it came from the closest possible enemy main
                        auto enemyMain = Map::getEnemyMain();
                        if (!enemyMain) enemyMain = Map::getEnemyStartingMain();
                        if (enemyMain)
                        {
                            framesMoved = PathFinding::ExpectedTravelTime(unit->lastPosition,
                                                                          enemyMain->getPosition(),
                                                                          unit->type,
                                                                          PathFinding::PathFindingOptions::UseNearestBWEMArea);
                        }
                        else
                        {
                            // Default to the closest unscouted starting position
                            framesMoved = INT_MAX;
                            for (auto base : Map::unscoutedStartingLocations())
                            {
                                int frames = PathFinding::ExpectedTravelTime(unit->lastPosition,
                                                                             base->getPosition(),
                                                                             unit->type,
                                                                             PathFinding::PathFindingOptions::UseNearestBWEMArea);
                                if (frames < framesMoved)
                                {
                                    framesMoved = frames;
                                }
                            }

                            // Guard against bugs in scouting
                            if (framesMoved == INT_MAX) framesMoved = 0;
                        }

                        // Give some leeway since the unit might have been produced away from the depot
                        // For Zerg we give less leeway since they have less building room
                        framesMoved = std::max(0, framesMoved - (unit->player->getRace() == BWAPI::Races::Zerg ? 75 : 200));
                    }
                }

                completionFrame = currentFrame - framesMoved;
            }

            int startFrame = completionFrame - UnitUtil::BuildTime(unit->type);
            enemyUnitTimings[unit->type].emplace_back(std::make_pair(startFrame, currentFrame));

            if (enemyUnitTimings[unit->type].size() == 1)
            {
                Log::Get() << "First enemy of type discovered: " << *unit;

                // Track some common tech rush units in our opponent model
                if (unit->type == BWAPI::UnitTypes::Protoss_Dark_Templar)
                {
                    Opponent::setGameValue("firstDarkTemplarCompleted", completionFrame);
                }
                else if (unit->type == BWAPI::UnitTypes::Zerg_Mutalisk)
                {
                    Opponent::setGameValue("firstMutaliskCompleted", completionFrame);
                }
                else if (unit->type == BWAPI::UnitTypes::Zerg_Lurker)
                {
                    // Get frames it would take this lurker to get to our main choke
                    auto mainChoke = Map::getMyMainChoke();
                    if (mainChoke)
                    {
                        auto frames = PathFinding::ExpectedTravelTime(unit->lastPosition,
                                                                      mainChoke->center,
                                                                      BWAPI::UnitTypes::Zerg_Lurker,
                                                                      PathFinding::PathFindingOptions::UseNeighbouringBWEMArea,
                                                                      1000);
                        Opponent::setGameValue("firstLurkerAtOurMain", completionFrame + frames);
                    }
                }
            }

            if (unit->type.isBuilding() && includeMorphs)
            {
                auto morphsFrom = UnitUtil::MorphsFrom(unit->type);
                if (morphsFrom.first != BWAPI::UnitTypes::None &&
                    morphsFrom.first != BWAPI::UnitTypes::Zerg_Drone)
                {
                    enemyUnitTimings[morphsFrom.first].emplace_back(std::make_pair(startFrame - UnitUtil::BuildTime(morphsFrom.first),
                                                                                   currentFrame));
                }
            }
        }

        void assignEnemyUnitsToBases()
        {
            // First clear dead units
            for (auto it = enemyUnitsToBase.begin(); it != enemyUnitsToBase.end();)
            {
                if (!it->first->exists())
                {
                    basesToEnemyUnits[it->second].erase(it->first);
                    it = enemyUnitsToBase.erase(it);
                }
                else
                {
                    it++;
                }
            }

            auto getBase = [](const Unit &unit) -> Base *
            {
                if (!unit->lastPositionValid) return nullptr;

                auto combatUnit =
                        (UnitUtil::IsCombatUnit(unit->type) || unit->lastSeenAttacking >= (currentFrame - 120))
                        && (unit->isTransport() || UnitUtil::CanAttackGround(unit->type));

                Base *closest = nullptr;
                int closestDist = 1000;
                for (auto &base : Map::allBases())
                {
                    // Only consider combat units and units that block untaken bases
                    if (!combatUnit &&
                        (base->owner != nullptr ||
                         !Geo::Overlaps(base->getTilePosition(), 4, 3, unit->getTilePosition(), 2, 2)))
                    {
                        continue;
                    }

                    // For our bases, ignore units we haven't seen for a while
                    if (base->owner == BWAPI::Broodwar->self() &&
                        !unit->type.isBuilding() &&
                        unit->lastSeen < (currentFrame - 240))
                    {
                        continue;
                    }

                    int dist = unit->isFlying
                               ? unit->lastPosition.getApproxDistance(base->getPosition())
                               : PathFinding::GetGroundDistance(unit->lastPosition, base->getPosition(), unit->type);
                    if (dist == -1 || dist > closestDist) continue;

                    // If the distance is above 500, ignore this unit if the base is unowned or it is moving away from the base
                    if (dist > 500)
                    {
                        if (!base->owner) continue;

                        auto predictedPosition = unit->predictPosition(1);
                        if (predictedPosition.isValid())
                        {
                            if (predictedPosition.getApproxDistance(base->getPosition()) >
                                unit->lastPosition.getApproxDistance(base->getPosition()))
                            {
                                continue;
                            }
                        }
                    }

                    closest = base;
                    closestDist = dist;
                }

                return closest;
            };

            // Now update each enemy unit
            for (auto &unit : enemyUnits)
            {
                auto base = getBase(unit);

                // Remove from current base if it is different from the new one
                auto current = enemyUnitsToBase.find(unit);
                if (current != enemyUnitsToBase.end())
                {
                    if (current->second == base) continue;

                    CherryVis::log(unit->id) << "Removed from base @ " << BWAPI::WalkPosition(current->second->getPosition());

                    basesToEnemyUnits[current->second].erase(unit);
                    enemyUnitsToBase.erase(current);
                }

                // Add to the new base if required
                if (base)
                {
                    CherryVis::log(unit->id) << "Added to base @ " << BWAPI::WalkPosition(base->getPosition());

                    basesToEnemyUnits[base].emplace(unit);
                    enemyUnitsToBase[unit] = base;
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
        enemyUnitsToBase.clear();
        basesToEnemyUnits.clear();
        upgradesInProgress.clear();
        researchInProgress.clear();

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
        researchInProgress.clear();

        auto ignoreUnit = [](BWAPI::Unit bwapiUnit)
        {
            return bwapiUnit->getType() == BWAPI::UnitTypes::Protoss_Interceptor ||
                   bwapiUnit->getType() == BWAPI::UnitTypes::Protoss_Scarab ||
                   bwapiUnit->getType().isSpell();
        };

        // Update our units
        // We always have vision of our own units, so we don't have to handle units in fog
        for (auto bwapiUnit : BWAPI::Broodwar->self()->getUnits())
        {
            trackResearch(bwapiUnit);

            if (ignoreUnit(bwapiUnit)) continue;

            // If we just mind controlled an enemy unit, consider the enemy unit destroyed
            auto enemyIt = unitIdToEnemyUnit.find(bwapiUnit->getID());
            if (enemyIt != unitIdToEnemyUnit.end())
            {
                enemyUnitDestroyed(enemyIt->second);
            }

            // Create or update
            MyUnit unit;
            auto it = unitIdToMyUnit.find(bwapiUnit->getID());
            if (it == unitIdToMyUnit.end() || !it->second || !it->second->exists())
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
                else if (bwapiUnit->getType() == BWAPI::UnitTypes::Protoss_Carrier)
                {
                    unit = std::make_shared<MyCarrier>(bwapiUnit);
                    unit->created();
                }
                else if (bwapiUnit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon)
                {
                    unit = std::make_shared<MyCannon>(bwapiUnit);
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
            else if (bwapiUnit->isResearching())
            {
                researchInProgress.insert(bwapiUnit->getTech());
            }

            if (bwapiUnit->getBuildUnit())
            {
                auto producing = unitIdToMyUnit.find(bwapiUnit->getBuildUnit()->getID());
                if (producing != unitIdToMyUnit.end())
                {
                    producing->second->producer = bwapiUnit;
                }
            }
        }

        // Update visible enemy units
        for (auto bwapiUnit : BWAPI::Broodwar->enemy()->getUnits())
        {
            trackResearch(bwapiUnit);

            if (ignoreUnit(bwapiUnit)) continue;
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
            if (unit->lastSeen == currentFrame) continue;

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

        // Update the order timers
        if ((BWAPI::Broodwar->getFrameCount() - 8) % 150 == 0)
        {
            for (auto &unit : myUnits) unit->orderProcessTimer = -1;
            for (auto &unit : enemyUnits) unit->orderProcessTimer = -1;
        }
        else
        {
            auto updateOrderProcessTimer = [](auto &unit)
            {
                if (unit->orderProcessTimer > 0)
                {
                    unit->orderProcessTimer--;
                }
                else if (unit->orderProcessTimer == 0)
                {
                    unit->orderProcessTimer = 8;
                }
            };
            for (auto &unit : myUnits) updateOrderProcessTimer(unit);
            for (auto &unit : enemyUnits) updateOrderProcessTimer(unit);
        }

        // Update visible neutral units to detect addons that have gone neutral or refineries that have become geysers
        destroyedEnemyUnits.clear();
        for (auto bwapiUnit : BWAPI::Broodwar->neutral()->getUnits())
        {
            if (!bwapiUnit->isVisible()) continue;

            auto it = unitIdToEnemyUnit.find(bwapiUnit->getID());
            if (it != unitIdToEnemyUnit.end())
            {
                auto &unit = it->second;

                // Refineries are treated as destroyed
                if (unit->type.isRefinery())
                {
                    destroyedEnemyUnits.push_back(unit);
                    continue;
                }

                // All others are treated as destroyed for the grid but otherwise not
                if (unit->type.isBuilding())
                {
                    Log::Get() << "Enemy switched to neutral: " << *unit;
                }

                if (unit->lastPositionValid && !unit->beingManufacturedOrCarried)
                {
                    Players::grid(unit->player).unitDestroyed(unit->type, unit->lastPosition, unit->completed, unit->burrowed, unit->immobile);
#if DEBUG_GRID_UPDATES
                    CherryVis::log(unit->id) << "Grid::unitDestroyed " << unit->lastPosition;
                    Log::Debug() << *unit << ": Grid::unitDestroyed " << unit->lastPosition;
#endif
                }

                unit->bwapiUnit = nullptr; // Signals to all holding a copy of the pointer that this unit is dead

                enemyUnits.erase(unit);
                unitIdToEnemyUnit.erase(it);
                enemyUnitsByType[unit->type].erase(unit);
            }
        }
        for (auto &unit : destroyedEnemyUnits)
        {
            enemyUnitDestroyed(unit);
        }

        assignEnemyUnitsToBases();

        // Occasionally check for any inconsistencies in the enemy unit collections
        if (currentFrame % 48 == 0)
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
#if DEBUG_DT_STATUS
            output = output || unit->type == BWAPI::UnitTypes::Protoss_Dark_Templar;
#endif
#if DEBUG_SHUTTLE_STATUS
            output = output || unit->type == BWAPI::UnitTypes::Protoss_Shuttle;
#endif
#if DEBUG_CARRIER_STATUS
            output = output || unit->type == BWAPI::UnitTypes::Protoss_Carrier;
#endif
#if DEBUG_OBSERVER_STATUS
            output = output || unit->type == BWAPI::UnitTypes::Protoss_Observer;
#endif
#if DEBUG_ARBITER_STATUS
            output = output || unit->type == BWAPI::UnitTypes::Protoss_Arbiter;
#endif
#if DEBUG_PRODUCINGBUILDING_STATUS
            output = output || (unit->type.isBuilding() && unit->type.canProduce());
#endif

            if (!output) continue;

            std::ostringstream debug;

            // First line is command
            debug << "cmd=" << unit->bwapiUnit->getLastCommand().getType() << ";f="
                  << (currentFrame - unit->lastCommandFrame);
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
                debug << "spd=" << ((int) (100.0 * speed / unit->type.topSpeed()))
                      << ";mvng=" << unit->bwapiUnit->isMoving() << ";rdy=" << unit->isReady()
                      << ";stk=" << unit->bwapiUnit->isStuck()
                      << ";lstmv=" << (unit->getLastMoveFrame() - currentFrame);
            }

            if (unit->getUnstickUntil() >= currentFrame)
            {
                debug << ";unstck=" << (unit->getUnstickUntil() - currentFrame);
            }

            if (unit->type == BWAPI::UnitTypes::Protoss_Dragoon)
            {
                debug << "\n";

                auto myDragoon = std::dynamic_pointer_cast<MyDragoon>(unit);
                debug << "lstatk=" << myDragoon->getLastAttackStartedAt();
                debug << ";nxtatk=" << myDragoon->getNextAttackPredictedAt();
                debug << ";stkf=" << myDragoon->getPotentiallyStuckSince();
            }

            if (unit->type.isBuilding() && unit->type.canProduce())
            {
                debug << ";q=[";
                bool first = true;
                for (const auto &type : unit->bwapiUnit->getTrainingQueue())
                {
                    if (first)
                    {
                        first = false;
                    }
                    else
                    {
                        debug << ",";
                    }
                    debug << type;
                }
                debug << "]";
            }

            CherryVis::log(unit->id) << debug.str();
        }

#if DEBUG_PREDICTED_POSITIONS
        auto outputPredictedPositions = [](const Unit& unit)
        {
            std::ostringstream buf;
            buf << "0: " << unit->lastPosition;
            for (int i=1; i <= BWAPI::Broodwar->getLatencyFrames() + 2; i++)
            {
                buf << "\n" << i << ": " << unit->predictPosition(i);
            }
            CherryVis::log(unit->id) << buf.str();
        };
        for (auto &unit : myUnits) outputPredictedPositions(unit);
        for (auto &unit : enemyUnits) outputPredictedPositions(unit);
#endif

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

        auto source = get(bullet->getSource());
        auto target = get(bullet->getTarget());
        if (!source || !target) return;

        source->lastTarget = target;

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
            if (source && target)
            {
                target->addUpcomingAttack(source, bullet);
            }
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

    std::unordered_set<MyUnit> &allMine()
    {
        return myUnits;
    }

    std::unordered_set<MyUnit> &allMineCompletedOfType(BWAPI::UnitType type)
    {
        return myCompletedUnitsByType[type];
    }

    std::unordered_set<MyUnit> &allMineIncompleteOfType(BWAPI::UnitType type)
    {
        return myIncompleteUnitsByType[type];
    }

    std::map<BWAPI::UnitType, std::unordered_set<MyUnit>> &allMineIncompleteByType()
    {
        return myIncompleteUnitsByType;
    }

    std::unordered_set<Unit> &allEnemy()
    {
        return enemyUnits;
    }

    std::unordered_set<Unit> &allEnemyOfType(BWAPI::UnitType type)
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
            if (unit->simPositionValid && unit->simPosition.getApproxDistance(position) <= radius)
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

    std::unordered_set<Unit> &enemyAtBase(Base *base)
    {
        return basesToEnemyUnits[base];
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

    bool isBeingUpgradedOrResearched(UpgradeOrTechType type)
    {
        if (type.isTechType())
        {
            return researchInProgress.find(type.techType) != researchInProgress.end();
        }

        return upgradesInProgress.find(type.upgradeType) != upgradesInProgress.end();
    }
}
