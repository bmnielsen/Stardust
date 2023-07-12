#include "McRave.h"

using namespace std;
using namespace BWAPI;
using namespace UnitTypes;

namespace McRave::Targets {

    set<UnitType> cancelPriority ={ Terran_Missile_Turret, Terran_Barracks, Terran_Bunker, Terran_Factory, Terran_Starport, Terran_Armory, Terran_Bunker };
    map<UnitType, double> allowedBuildings;

    namespace {

        bool allowThreatenTarget(UnitInfo& unit, UnitInfo& target)
        {
            if (unit.getType() == Zerg_Mutalisk) {
                return (Spy::getEnemyTransition() != "4Gate" || Util::getTime() > Time(10, 00));
            }
            return true;
        }

        bool allowWorkerTarget(UnitInfo& unit, UnitInfo& target) {
            if (target.getType().isWorker() && Util::getTime() < Time(8, 00)) {
                return unit.getType().isWorker()
                    || Spy::getEnemyTransition() == "WorkerRush"
                    || target.hasAttackedRecently()
                    || unit.getGroundRange() > 32.0
                    || target.isThreatening()
                    || (target.getUnitsTargetingThis().empty() && !Players::ZvZ())
                    || unit.attemptingRunby()
                    || Terrain::inTerritory(PlayerState::Enemy, target.getPosition());
            }
            return true;
        }

        bool isValidTarget(UnitInfo& unit, UnitInfo& target)
        {
            if (!target.unit()
                || target.isInvincible()
                || !target.getWalkPosition().isValid())
                return false;
            return true;
        }

        bool suitableTargetWorker(UnitInfo& unit, UnitInfo& target)
        {
            return false;
        }

        bool suitableTargetBuilding(UnitInfo& unit, UnitInfo& target)
        {
            return false;
        }

        bool suitableTargetUnit(UnitInfo& unit, UnitInfo& target)
        {
            return false;
        }

        bool isSuitableTarget(UnitInfo& unit, UnitInfo& target)
        {
            if (target.movedFlag)
                return false;

            auto &enemyStrength = Players::getStrength(PlayerState::Enemy);
            auto &myStrength = Players::getStrength(PlayerState::Self);

            bool targetCanAttack = !unit.isHidden() && (((unit.getType().isFlyer() && target.getAirDamage() > 0.0) || (!unit.getType().isFlyer() && target.canAttackGround()) || (!unit.getType().isFlyer() && target.getType() == Terran_Vulture_Spider_Mine)));
            bool unitCanAttack = !target.isHidden() && ((target.isFlying() && unit.getAirDamage() > 0.0) || (!target.isFlying() && unit.canAttackGround()) || (unit.getType() == Protoss_Carrier));
            bool enemyHasGround = enemyStrength.groundToAir > 0.0 || enemyStrength.groundToGround > 0.0 || enemyStrength.groundDefense > 0.0;
            bool enemyHasAir = enemyStrength.airToGround > 0.0 || enemyStrength.airToAir > 0.0 || enemyStrength.airDefense > 0.0;
            auto enemyCanHitAir = enemyStrength.groundToAir > 0.0 || enemyStrength.airToAir > 0.0 || enemyStrength.airDefense > 0.0;
            bool selfHasGround = myStrength.groundToAir > 0.0 || myStrength.groundToGround > 0.0;
            bool selfHasAir = myStrength.airToGround > 0.0 || myStrength.airToAir > 0.0;

            bool enemyCanDefendUnit = unit.isFlying() ? enemyStrength.airDefense > 0.0 : enemyStrength.groundDefense > 0.0;

            // Check if the target is important right now to attack
            bool targetMatters = target.getType().isSpell()
                || (target.canAttackAir() && selfHasAir)
                || (target.canAttackGround())
                || (target.getType().spaceProvided() > 0)
                || (target.getType().isDetector())
                || (!target.canAttackAir() && !target.canAttackGround() && !unit.hasTransport())
                || (target.getType().isWorker())
                || (!enemyHasGround && !enemyHasAir)
                || (Players::ZvZ() && enemyCanDefendUnit)
                || (Players::ZvZ() && Spy::enemyFastExpand())
                || Util::getTime() > Time(6, 00)
                || Spy::enemyGreedy()
                || (target.getType() == Protoss_Pylon && target.isProxy() && Spy::getEnemyTransition() == "ZealotRush");

            if (!unitCanAttack)
                return false;

            // Combat Role
            if (unit.getRole() == Role::Combat) {

                // Generic
                if (!targetMatters
                    || (target.getType() == Terran_Vulture_Spider_Mine && int(target.getUnitsTargetingThis().size()) >= 4 && !target.isBurrowed())                          // Don't over target spider mines
                    || (target.getType() == Protoss_Interceptor && unit.isFlying())                                                                                         // Don't target interceptors as a flying unit
                    || (!allowWorkerTarget(unit, target))
                    || (target.isHidden() && (!targetCanAttack || (!Players::hasDetection(PlayerState::Self) && Players::PvP())) && !unit.getType().isDetector())           // Don't target if invisible and can't attack this unit or we have no detectors in PvP
                    || (target.isFlying() && !unit.isFlying() && !BWEB::Map::isWalkable(target.getTilePosition(), unit.getType()) && !unit.isWithinRange(target))           // Don't target flyers that we can't reach
                    || (target.getType().isBuilding() && !target.canAttackGround() && !target.canAttackAir() && !target.isThreatening() && (target.getType() != Protoss_Pylon || !target.isProxy()) && Util::getTime() < Time(5, 00))     // Don't attack buildings that aren't threatening early on
                    || (unit.isSpellcaster() && (target.getType() == Terran_Vulture_Spider_Mine || target.getType().isBuilding()))                                          // Don't cast spells on mines or buildings
                    || (unit.isLightAir() && target.getType() == Protoss_Interceptor)
                    || (unit.getType().isWorker() && target.getType() == Terran_Bunker && !selfHasGround))
                    return false;

                // Zergling
                if (unit.getType() == Zerg_Zergling) {
                    if (Util::getTime() < Time(5, 00)) {
                        if ((Players::ZvZ() && !target.canAttackGround() && !Spy::enemyFastExpand())                                                                 // Avoid non ground hitters to try and kill drones
                            || (Players::ZvT() && target.getType().isWorker() && !target.isThreatening() && Spy::getEnemyBuild() == "RaxFact")                       // Avoid workers when we need to prepare for runbys|| 
                            || (BuildOrder::isProxy() && !target.isThreatening() && !target.getType().isWorker() && Util::getTime() < Time(6, 00)))
                            return false;
                    }
                    if (unit.attemptingRunby() && (!target.getType().isWorker() || !Terrain::inTerritory(PlayerState::Enemy, target.getPosition())))
                        return false;
                    if (target.getUnitsTargetingThis().size() >= 4 && target.getType() == Protoss_Zealot)
                        return false;
                }

                // Mutalisk
                if (unit.getType() == Zerg_Mutalisk) {
                    auto anythingTime = Util::getTime() > (Players::ZvZ() ? Time(7, 00) : Time(12, 00));
                    auto anythingSupply = !Players::ZvZ() && Players::getSupply(PlayerState::Enemy, Races::None) < 20;
                    auto defendExpander = BuildOrder::shouldExpand() && unit.getGoal().isValid();
                    auto invalidType = allowedBuildings.find(target.getType()) == allowedBuildings.end();

                    if (target.isThreatening())
                        return allowThreatenTarget(unit, target);

                    if (!defendExpander && invalidType && !anythingTime && !anythingSupply && unit.attemptingHarass()
                        && !unit.canOneShot(target) && !target.getType().isWorker() && !target.isLightAir() && !unit.globalRetreat()) {

                        if ((enemyHasAir && !target.getType().isWorker() && !target.canAttackAir() && enemyHasGround)                                                                   // Avoid non-air shooters
                            || (!target.getType().isWorker() && !target.getType().isBuilding() && !target.canAttackAir())                                                               // Avoid ground fighters only that we can't oneshot
                            || (target.getType().isBuilding() && enemyCanHitAir && !target.canAttackAir() && !target.canAttackGround())                                                 // Avoid buildings that don't fight
                            || (!unit.canTwoShot(target) && !target.isFlying() && !target.getType().isBuilding()))                                                                      // Avoid units we can't 2 shot
                            return false;
                    }
                }

                // Lurker
                if (unit.getType() == Zerg_Lurker) {
                    if (!target.getType().isWorker() && BuildOrder::isProxy())
                        return false;
                }

                // Scourge
                if (unit.getType() == Zerg_Scourge) {
                    if ((!Stations::getStations(PlayerState::Enemy).empty() && target.getType().isBuilding())                                                                            // Avoid targeting buildings if the enemy has a base still
                        || target.getType() == Zerg_Overlord                                                                                                                // Don't target overlords
                        || target.getType() == Protoss_Interceptor)                                                                                                         // Don't target interceptors
                        return false;
                }

                // Defiler
                if (unit.getType() == Zerg_Defiler) {
                    if (target.getType().isBuilding()
                        || target.getType().isWorker()
                        || (unit.targetsFriendly() && target.getType() != Zerg_Zergling)
                        || target.isFlying())
                        return false;
                }

                // Zealot
                if (unit.getType() == Protoss_Zealot) {
                    if ((target.getType() == Terran_Vulture_Spider_Mine && Broodwar->self()->getUpgradeLevel(UpgradeTypes::Protoss_Ground_Weapons) < 2)                     // Avoid mines without +2 to oneshot them
                        || ((target.getSpeed() > unit.getSpeed() || target.getType().isBuilding()) && !target.getType().isWorker() && BuildOrder::isRush()))                // Avoid faster units when we're rushing
                        return false;
                }

                // Dark Templar
                if (unit.getType() == Protoss_Dark_Templar) {
                    if (target.getType() == Terran_Vulture && !unit.isWithinRange(target))                                                                                  // Avoid vultures if not in range already
                        return false;
                }
            }

            if (unit.getRole() == Role::Scout) {
                if (!target.getType().isWorker() && !target.isThreatening())
                    return false;
            }
            return true;
        }

        double scoreTarget(UnitInfo& unit, UnitInfo& target)
        {
            // Scoring parameters
            auto range =          target.getType().isFlyer() ? unit.getAirRange() : unit.getGroundRange();
            auto reach =          target.getType().isFlyer() ? unit.getAirReach() : unit.getGroundReach();
            const auto boxDistance =    double(Util::boxDistance(unit.getType(), unit.getPosition(), target.getType(), target.getPosition()));
            const auto dist =           exp(0.0125 * boxDistance);

            if (unit.isSiegeTank()) {
                range = 384.0;
                reach = range + (unit.getSpeed() * 16.0) + double(unit.getType().width() / 2) + 64.0;
            }

            const auto bonusScore = [&]() {

                // Add bonus for expansion killing
                if (target.getType().isResourceDepot() && !Players::ZvZ() && (Util::getTime() > Time(8, 00) || Spy::enemyGreedy()) && !unit.isLightAir())
                    return 5000.0;

                // Add bonus for a building that is undefended
                if (unit.isLightAir() && target.getType().isBuilding() && allowedBuildings.find(target.getType()) != allowedBuildings.end() && ((unit.getUnitsInRangeOfThis().empty() && unit.isWithinRange(target)) || target.canAttackAir())) {

                    auto closestDefense = Util::getClosestUnit(target.getPosition(), PlayerState::Enemy, [&](auto &u) {
                        return u->getType().isBuilding() && u->canAttackAir();
                    });
                    if (!closestDefense || closestDefense->getPosition().getDistance(target.getPosition()) > 96.0)
                        return allowedBuildings[target.getType()] * (1.0 + double(target.unit()->exists() && target.unit()->isUpgrading()));
                }

                // Add penalty for targeting workers under defenses
                if (unit.isLightAir() && target.getType().isWorker()) {
                    auto countDamageInRange = 0;
                    for (auto &e : Units::getUnits(PlayerState::Enemy)) {
                        auto viableThreat = e->getType().isBuilding() || Broodwar->getFrameCount() - e->getLastVisibleFrame() < 200;
                        if (e->canAttackAir() && viableThreat && *e != target && (e->getPosition().getDistance(target.getPosition()) < e->getAirRange() + 32.0 || e->getPosition().getDistance(unit.getPosition()) < e->getAirRange() + 32.0)) {
                            auto damage = e->getAirDamage() / (e->getType().airWeapon().damageType() == DamageTypes::Explosive ? 2 : 1);
                            countDamageInRange += damage;
                        }
                    }
                    if ((countDamageInRange >= 50 && Util::getTime() < Time(8, 00))
                        || (countDamageInRange >= 70 && Util::getTime() < Time(16, 00)))
                        return 0.0;
                }

                // Add bonus for Observers that are vulnerable
                if (target.getType() == Protoss_Observer && !target.isHidden())
                    return 20.0;

                // Add penalty for a Terran building that's being constructed
                if (target.getType().isBuilding() && target.getType().getRace() == Races::Terran && !target.unit()->isCompleted() && !target.isThreatening() && !target.unit()->getBuildUnit())
                    return 0.1;

                // Add penalty for a Terran building that has been repaired recently
                if (target.getType().isBuilding() && target.hasRepairedRecently())
                    return 0.1;

                // Add bonus for an SCV repairing a turret or building a turret
                if (target.getType().isWorker() && ((target.unit()->isConstructing() && target.unit()->getBuildUnit() && cancelPriority.find(target.unit()->getBuildUnit()->getType()) != cancelPriority.end()) || (target.hasRepairedRecently())))
                    return 10.0;

                // Add bonus for a building warping in
                if (Util::getTime() > Time(8, 00) && target.unit()->exists() && !target.unit()->isCompleted() && (target.getType() == Protoss_Photon_Cannon || (target.getType() == Terran_Missile_Turret && !target.unit()->getBuildUnit())))
                    return 25.0;

                // Add bonus for a DT to kill important counters
                if (unit.getType() == Protoss_Dark_Templar && unit.isWithinReach(target) && !Actions::overlapsDetection(target.unit(), target.getPosition(), PlayerState::Enemy)
                    && (target.getType().isWorker() || target.isSiegeTank() || target.getType().isDetector() || target.getType() == Protoss_Observatory || target.getType() == Protoss_Robotics_Facility))
                    return 20.0;

                // Fuck cannons
                if (target.getType() == UnitTypes::Protoss_Photon_Cannon && Util::getTime() > Time(10, 00))
                    return 5.0;

                // Add bonus for being able to one shot a unit
                if (!Players::ZvZ() && unit.canOneShot(target) && Util::getTime() < Time(10, 00))
                    return 4.0;

                // Add bonus for being able to two shot a unit
                if (!Players::ZvZ() && unit.canTwoShot(target) && Util::getTime() < Time(10, 00))
                    return 2.0;
                return 1.0;
            };

            const auto healthScore = [&]() {
                if (range > 32.0 && target.unit()->isCompleted())
                    return 1.0 + (0.1 * (1.0 - target.getPercentTotal()));
                return 1.0;
            };

            const auto focusScore = [&]() {
                if (unit.isLightAir())
                    return 1.0;
                if ((range > 32.0 && boxDistance <= reach) || (range <= 32.0 && target.getUnitsTargetingThis().size() < 4 && boxDistance <= range))
                    return (1.0 + double(target.getUnitsTargetingThis().size()));
                return 1.0;
            };

            const auto priorityScore = [&]() {
                if (!target.getType().isWorker() && !target.isThreatening() && ((!target.canAttackAir() && unit.isFlying()) || (!target.canAttackGround() && !unit.isFlying())))
                    return target.getPriority() / 4.0;
                if (target.getType().isWorker() && !Terrain::inTerritory(PlayerState::Enemy, target.getPosition()))
                    return target.getPriority() / 10.0;
                return target.getPriority();
            };

            const auto effectiveness = [&]() {
                auto weapon = target.isFlying() ? unit.getType().airWeapon() : unit.getType().groundWeapon();
                if (weapon.damageType() == DamageTypes::Explosive) {
                    if (target.getType().size() == UnitSizeTypes::Small)
                        return 0.5;
                    if (target.getType().size() == UnitSizeTypes::Medium)
                        return 0.75;
                }
                else if (weapon.damageType() == DamageTypes::Concussive) {
                    if (target.getType().size() == UnitSizeTypes::Medium)
                        return 0.5;
                    if (target.getType().size() == UnitSizeTypes::Large)
                        return 0.25;
                }
                return 1.0;
            };

            const auto targetScore = healthScore() * focusScore() * priorityScore() * bonusScore();

            // Detector targeting (distance to nearest ally added)
            if ((target.isBurrowed() || target.unit()->isCloaked()) && ((unit.getType().isDetector() && !unit.getType().isBuilding()) || unit.getType() == Terran_Comsat_Station)) {
                auto closest = Util::getClosestUnit(target.getPosition(), PlayerState::Self, [&](auto &u) {
                    return *u != unit && u->getRole() == Role::Combat && target.getType().isFlyer() ? u->getAirRange() > 0 : u->getGroundRange() > 0;
                });

                // Detectors want to stay close to their target if we have a friendly nearby
                if (closest && closest->getPosition().getDistance(target.getPosition()) < 480.0)
                    return priorityScore() / (dist * closest->getPosition().getDistance(target.getPosition()));
                return 0.0;
            }

            // Cluster targeting (grid score added)
            else if (unit.getType() == Protoss_High_Templar || unit.getType() == Protoss_Arbiter) {
                const auto eGrid =      Grids::getEGroundCluster(target.getWalkPosition()) + Grids::getEAirCluster(target.getWalkPosition());
                const auto aGrid =      1.0 + Grids::getAGroundCluster(target.getWalkPosition()) + Grids::getAAirCluster(target.getWalkPosition());
                const auto gridScore =  eGrid / aGrid;
                return gridScore * targetScore / dist;
            }

            // Defender targeting (distance not used)
            else if (unit.getRole() == Role::Defender && boxDistance - range <= 16.0) {
                if (unit.unit()->isSelected()) {
                    Broodwar->drawTextMap(target.getPosition(), "%.2f", healthScore() * priorityScore() * effectiveness());
                }
                return healthScore() * priorityScore() * effectiveness();
            }

            // Lurker burrowed targeting (only distance matters)
            else if (unit.getType() == Zerg_Lurker && unit.isBurrowed())
                return 1.0 / dist;

            // Proximity targeting (targetScore not used)
            else if (unit.getType() == Protoss_Reaver || unit.getType() == Zerg_Ultralisk) {
                if (target.getType().isBuilding() && !target.canAttackGround() && !target.canAttackAir())
                    return 0.1 / dist;
                return 1.0 / dist;
            }
            return targetScore / dist;
        }

        void getSimTarget(UnitInfo& unit, PlayerState pState)
        {
            double distBest = DBL_MAX;
            for (auto &t : Units::getUnits(pState)) {
                UnitInfo &target = *t;
                auto targetCanAttack = ((unit.isFlying() && target.getAirDamage() > 0.0) || (!unit.isFlying() && target.canAttackGround()) || (!unit.getType().isFlyer() && target.getType() == Terran_Vulture_Spider_Mine));
                auto unitCanAttack = ((target.isFlying() && unit.getAirDamage() > 0.0) || (!target.isFlying() && unit.canAttackGround()) || (unit.getType() == Protoss_Carrier));

                if (!targetCanAttack && (!unit.hasTarget() || target != unit.getTarget()))
                    continue;

                if (!target.isWithinReach(unit))
                    continue;

                if (!allowWorkerTarget(unit, target))
                    continue;

                // Set sim position
                auto dist = unit.getPosition().getDistance(target.getPosition()) * (1 + int(!targetCanAttack));
                if (dist < distBest) {
                    unit.setSimTarget(&target);
                    distBest = dist;
                }
            }

            if (!unit.hasSimTarget() && unit.hasTarget())
                unit.setSimTarget(&*unit.getTarget().lock());
        }

        void getBestTarget(UnitInfo& unit, PlayerState pState)
        {
            const auto checkGroundAccess = [&](UnitInfo& target) {
                if (unit.isFlying())
                    return true;

                if (target.lastUnreachableFrame > Broodwar->getFrameCount() - 96 && !target.isWithinRange(unit) && !unit.isWithinRange(target))
                    return false;

                if (target.isFlying() && BWEB::Map::isWalkable(target.getTilePosition(), Zerg_Ultralisk)) {
                    auto grdDist = BWEB::Map::getGroundDistance(unit.getPosition(), target.getPosition());
                    auto airDist = unit.getPosition().getDistance(target.getPosition());
                    if (grdDist > airDist * 2)
                        return false;
                }
                return true;
            };

            auto scoreBest = 0.0;
            for (auto &t : Units::getUnits(pState)) {
                UnitInfo &target = *t;

                if (!isValidTarget(unit, target)
                    || !isSuitableTarget(unit, target))
                    continue;

                // If this target is more important to target, set as current target
                const auto thisUnit = scoreTarget(unit, target);
                if (thisUnit > scoreBest && checkGroundAccess(target)) {
                    scoreBest = thisUnit;
                    unit.setTarget(&target);
                }
            }

            // Check for any self targets marked for death
            for (auto &t : Units::getUnits(PlayerState::Self)) {
                UnitInfo &target = *t;

                if (!isValidTarget(unit, target)
                    || !target.isMarkedForDeath()
                    || unit.isLightAir())
                    continue;

                // If this target is more important to target, set as current target
                const auto thisUnit = scoreTarget(unit, target);
                if (thisUnit > scoreBest && checkGroundAccess(target)) {
                    scoreBest = thisUnit;
                    unit.setTarget(&target);
                }
            }

            // Check for any neutral targets marked for death
            for (auto &t : Units::getUnits(PlayerState::Neutral)) {
                UnitInfo &target = *t;

                if (!isValidTarget(unit, target)
                    || !target.isMarkedForDeath()
                    || unit.isLightAir())
                    continue;

                // If this target is more important to target, set as current target
                const auto thisUnit = scoreTarget(unit, target);
                if (thisUnit > scoreBest && checkGroundAccess(target)) {
                    scoreBest = thisUnit;
                    unit.setTarget(&target);
                }
            }

            // Add this unit to the targeted by vector
            if (unit.hasTarget() && unit.getRole() == Role::Combat)
                unit.getTarget().lock()->getUnitsTargetingThis().push_back(unit.weak_from_this());
        }

        void getTarget(UnitInfo& unit)
        {
            auto pState = unit.targetsFriendly() ? PlayerState::Self : PlayerState::Enemy;

            // Spider mines have a set order target, possibly scarabs too
            if (unit.getType() == Terran_Vulture_Spider_Mine) {
                if (unit.unit()->getOrderTarget())
                    unit.setTarget(&*Units::getUnitInfo(unit.unit()->getOrderTarget()));
            }

            // Self units are assigned targets
            if (unit.getRole() == Role::Combat || unit.getRole() == Role::Support || unit.getRole() == Role::Defender || unit.getRole() == Role::Worker || unit.getRole() == Role::Scout) {
                getBestTarget(unit, pState);
                getSimTarget(unit, PlayerState::Enemy);
            }

            // Enemy units are assumed targets or order targets
            if (unit.getPlayer()->isEnemy(Broodwar->self())) {
                auto &targetInfo = unit.unit()->getOrderTarget() ? Units::getUnitInfo(unit.unit()->getOrderTarget()) : nullptr;
                if (targetInfo) {
                    unit.setTarget(&*targetInfo);
                    targetInfo->getUnitsTargetingThis().push_back(unit.weak_from_this());
                }
                else if (unit.getType() != Terran_Vulture_Spider_Mine) {
                    auto closest = Util::getClosestUnit(unit.getPosition(), PlayerState::Self, [&](auto &u) {
                        return (u->isFlying() && unit.getAirDamage() > 0.0) || (!u->isFlying() && unit.getGroundDamage() > 0.0);
                    });
                    if (closest)
                        unit.setTarget(&*closest);
                }
            }
        }

        void updateTargets()
        {
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                UnitInfo& unit = *u;
                getTarget(unit);
            }
            for (auto &u : Units::getUnits(PlayerState::Enemy)) {
                UnitInfo& unit = *u;
                getTarget(unit);
            }
        }

        void updateAllowedBuildings()
        {
            if (Util::getTime() > Time(8, 00)) {
                allowedBuildings[Protoss_Cybernetics_Core]= 50000.0;
                allowedBuildings[Protoss_Forge]= 50000.0;
                allowedBuildings[Protoss_Photon_Cannon]= 100000.0;
                allowedBuildings[Terran_Armory]= 150000.0;
                allowedBuildings[Terran_Missile_Turret]= 50000.0;
                allowedBuildings[Zerg_Spire]= 50000.0;
                allowedBuildings[Zerg_Spawning_Pool]= 50000.0;
            }
            if (Util::getTime() > Time(10, 00)) {
                allowedBuildings[Protoss_Gateway]= 50000.0;
                allowedBuildings[Protoss_Pylon]= 50000.0;
                allowedBuildings[Protoss_Stargate]= 50000.0;
                allowedBuildings[Terran_Engineering_Bay]= 50000.0;
                allowedBuildings[Terran_Missile_Turret]= 50000.0;
                allowedBuildings[Terran_Supply_Depot]= 50000.0;
            }
        }
    }

    void onFrame()
    {
        updateAllowedBuildings();
        updateTargets();
    }
}