#include "McRave.h"

using namespace std;
using namespace BWAPI;
using namespace UnitTypes;

namespace McRave::Targets {

    set<UnitType> cancelPriority ={ Terran_Missile_Turret, Terran_Barracks, Terran_Factory, Terran_Starport, Terran_Armory, Terran_Bunker };
    set<UnitType> allowedBuildings ={ Terran_Armory, Protoss_Cybernetics_Core, Protoss_Photon_Cannon, Zerg_Spire };

    namespace {

        bool isValidTarget(UnitInfo& unit, UnitInfo& target)
        {
            if (!target.unit()
                || target.unit()->isInvincible()
                || !target.getWalkPosition().isValid()
                || target.unit()->isStasised()
                || (!unit.isFlying() && target.lastUnreachableFrame > Broodwar->getFrameCount() - 96 && !target.isWithinRange(unit) && !unit.isWithinRange(target)))
                return false;
            return true;
        }

        bool suitableTargetWorker(UnitInfo& unit, UnitInfo& target)
        {

        }

        bool suitableTargetBuilding(UnitInfo& unit, UnitInfo& target)
        {

        }

        bool suitableTargetUnit(UnitInfo& unit, UnitInfo& target)
        {

        }

        bool isSuitableTarget(UnitInfo& unit, UnitInfo& target)
        {
            if (target.movedFlag)
                return false;

            auto enemyStrength = Players::getStrength(PlayerState::Enemy);
            auto myStrength = Players::getStrength(PlayerState::Self);

            bool targetCanAttack = !unit.isHidden() && (((unit.getType().isFlyer() && target.getAirDamage() > 0.0) || (!unit.getType().isFlyer() && target.canAttackGround()) || (!unit.getType().isFlyer() && target.getType() == Terran_Vulture_Spider_Mine)));
            bool unitCanAttack = !target.isHidden() && ((target.isFlying() && unit.getAirDamage() > 0.0) || (!target.isFlying() && unit.canAttackGround()) || (unit.getType() == Protoss_Carrier));
            bool enemyHasGround = enemyStrength.groundToAir > 0.0 || enemyStrength.groundToGround > 0.0 || enemyStrength.groundDefense > 0.0;
            bool enemyHasAir = enemyStrength.airToGround > 0.0 || enemyStrength.airToAir > 0.0 || enemyStrength.airDefense > 0.0;
            bool selfHasGround = myStrength.groundToAir > 0.0 || myStrength.groundToGround > 0.0;
            bool selfHasAir = myStrength.airToGround > 0.0 || myStrength.airToAir > 0.0;

            bool atHome = Terrain::isInAllyTerritory(target.getTilePosition());
            bool atEnemy = Terrain::isInEnemyTerritory(target.getTilePosition());

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
                || (Players::ZvZ() && Strategy::enemyFastExpand())
                || Util::getTime() > Time(6, 00)
                || Strategy::enemyGreedy()
                || (target.getType() == Protoss_Pylon && target.isProxy() && Strategy::getEnemyTransition() == "ZealotRush");

            // Combat Role
            if (unit.getRole() == Role::Combat) {

                // Generic
                if (!unitCanAttack
                    || !targetMatters
                    || (target.getType() == Terran_Vulture_Spider_Mine && int(target.getTargetedBy().size()) >= 4 && !target.isBurrowed())                                  // Don't over target spider mines
                    || (target.getType() == Protoss_Interceptor && unit.isFlying())                                                                                         // Don't target interceptors as a flying unit
                    || (target.getType().isWorker() && !unit.getType().isWorker() && (Strategy::getEnemyTransition() != "WorkerRush" || !atHome) && !target.hasAttackedRecently() && unit.getGroundRange() <= 32.0 && !target.isThreatening() && (!target.getTargetedBy().empty() || Players::ZvZ()) && !atEnemy && Util::getTime() < Time(8, 00))                            // Don't target non threatening workers in our territory
                    || (target.isHidden() && (!targetCanAttack || (!Players::hasDetection(PlayerState::Self) && Players::PvP())) && !unit.getType().isDetector())           // Don't target if invisible and can't attack this unit or we have no detectors in PvP
                    || (target.isFlying() && !unit.isFlying() && !BWEB::Map::isWalkable(target.getTilePosition(), unit.getType()) && !unit.isWithinRange(target))           // Don't target flyers that we can't reach
                    || (target.getType().isBuilding() && !target.canAttackGround() && !target.canAttackAir() && atHome && !target.isThreatening() && (target.getType() != Protoss_Pylon || !target.isProxy()) && Util::getTime() < Time(5, 00))     // Don't attack buildings that aren't threatening early on
                    || (unit.isSpellcaster() && (target.getType() == Terran_Vulture_Spider_Mine || target.getType().isBuilding()))                                          // Don't cast spells on mines or buildings
                    || (unit.isLightAir() && target.getType() == Protoss_Interceptor)
                    || (unit.getType().isWorker() && target.getType() == Terran_Bunker && !selfHasGround))
                    return false;

                // Zergling
                if (unit.getType() == Zerg_Zergling) {
                    if (Util::getTime() < Time(5, 00)) {
                        if ((Players::ZvZ() && !target.canAttackGround() && !Strategy::enemyFastExpand())                                                                 // Avoid non ground hitters to try and kill drones
                            || (Players::ZvT() && target.getType().isWorker() && !target.isThreatening() && Strategy::getEnemyBuild() == "RaxFact")                       // Avoid workers when we need to prepare for runbys
                            || (BuildOrder::isProxy() && !target.isThreatening() && !target.getType().isWorker() && Util::getTime() < Time(6, 00)))
                            return false;
                    }
                    if (unit.attemptingRunby() && !target.getType().isWorker())
                        return false;
                }

                // Mutalisk
                if (unit.getType() == Zerg_Mutalisk) {
                    auto anythingTime = Players::ZvZ() ? Time(7, 00) : Time(12, 00);
                    auto defendExpander = BuildOrder::shouldExpand() && target.getType() == Terran_Vulture;

                    if (!defendExpander && Util::getTime() < anythingTime && unit.attemptingHarass() && !target.isThreatening() && !unit.canOneShot(target) && !target.getType().isWorker() && !target.isLightAir()) {
                        auto invalidBuilding = target.getType().isBuilding() && allowedBuildings.find(target.getType()) == allowedBuildings.end() && (target.unit()->isCompleted() || !target.canAttackAir());

                        if ((enemyHasAir && !target.getType().isWorker() && !target.canAttackAir() && enemyHasGround)                                                                   // Avoid non-air shooters
                            || (!target.getType().isWorker() && !target.getType().isBuilding() && !target.canAttackAir())                                                               // Avoid ground fighters only that we can't oneshot
                            || (target.getType().isBuilding() && enemyHasAir && !target.canAttackAir() && !target.canAttackGround())                                                    // Avoid buildings that don't fight
                            || (invalidBuilding && Grids::getAAirCluster(unit.getPosition()) <= 24.0f)                                                                                  // Avoid completed buildings that don't fight without a medium cluster
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
                    if ((!Stations::getEnemyStations().empty() && target.getType().isBuilding())                                                                            // Avoid targeting buildings if the enemy has a base still
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

            // HACK to not target air units over cliffs or in areas technically far away
            if (!unit.isFlying() && target.isFlying() && BWEB::Map::isWalkable(target.getTilePosition(), Zerg_Ultralisk)) {
                auto grdDist = BWEB::Map::getGroundDistance(unit.getPosition(), target.getPosition());
                auto airDist = unit.getPosition().getDistance(target.getPosition());
                if (grdDist > airDist * 2)
                    return 0.0;
            }

            const auto bonusScore = [&]() {

                // Add bonus for expansion killing
                if (target.getType().isResourceDepot() && (Util::getTime() > Time(8, 00) || Strategy::enemyGreedy()) && !unit.isLightAir())
                    return 5000.0;

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
                if (target.unit()->exists() && !target.unit()->isCompleted() && (target.getType() == Protoss_Photon_Cannon || (target.getType() == Terran_Missile_Turret && !target.unit()->getBuildUnit())))
                    return 25.0;

                // Add bonus for a DT to kill important counters
                if (unit.getType() == Protoss_Dark_Templar && unit.isWithinReach(target) && !Actions::overlapsDetection(target.unit(), target.getPosition(), PlayerState::Enemy)
                    && (target.getType().isWorker() || target.isSiegeTank() || target.getType().isDetector() || target.getType() == Protoss_Observatory || target.getType() == Protoss_Robotics_Facility))
                    return 20.0;

                // Add bonus for being able to one shot a unit
                if (unit.canOneShot(target))
                    return 4.0;

                // Add bonus for being able to two shot a unit
                if (unit.canTwoShot(target))
                    return 2.0;
                return 1.0;
            };

            const auto healthScore = [&]() {
                const auto healthRatio = unit.isLightAir() ? 1.0 : (1.0 - target.getPercentTotal());
                if (range > 32.0 && target.unit()->isCompleted())
                    return 1.0 + (0.1*healthRatio);
                return 1.0;
            };

            const auto focusScore = [&]() {
                if (range > 32.0 && boxDistance <= reach)
                    return exp(int(target.getTargetedBy().size()) + 1);
                if (range <= 32.0 && int(target.getTargetedBy().size()) >= 6)
                    return 1.0 / (int(target.getTargetedBy().size()));
                return 1.0;
            };

            const auto priorityScore = [&]() {
                if (!target.getType().isWorker() && ((!target.canAttackAir() && unit.isFlying()) || (!target.canAttackGround() && !unit.isFlying())))
                    return target.getPriority() / 4.0;
                if (target.getType().isWorker() && unit.isLightAir() && Grids::getEAirThreat(unit.getPosition()) > 0.0f)
                    return target.getPriority() / 4.0;
                return target.getPriority();
            };

            const auto targetScore = healthScore() * focusScore() * priorityScore() * bonusScore();

            // Detector targeting (distance to nearest ally added)
            if ((target.isBurrowed() || target.unit()->isCloaked()) && ((unit.getType().isDetector() && !unit.getType().isBuilding()) || unit.getType() == Terran_Comsat_Station)) {
                auto closest = Util::getClosestUnit(target.getPosition(), PlayerState::Self, [&](auto &u) {
                    return u != unit && u.getRole() == Role::Combat && target.getType().isFlyer() ? u.getAirRange() > 0 : u.getGroundRange() > 0;
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
            else if (unit.getRole() == Role::Defender && boxDistance - range <= 16.0)
                return healthScore() * priorityScore();

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

                // Set sim position
                auto dist = unit.getPosition().getDistance(target.getPosition()) * (1 + int(!targetCanAttack));
                if (dist < distBest) {
                    unit.setSimTarget(&target);
                    distBest = dist;
                }
            }

            if (!unit.hasSimTarget() && unit.hasTarget())
                unit.setSimTarget(&unit.getTarget());
        }

        void getBestTarget(UnitInfo& unit, PlayerState pState)
        {
            auto scoreBest = 0.0;
            for (auto &t : Units::getUnits(pState)) {
                UnitInfo &target = *t;

                if (!isValidTarget(unit, target)
                    || !isSuitableTarget(unit, target))
                    continue;

                // If this target is more important to target, set as current target
                const auto thisUnit = scoreTarget(unit, target);
                if (thisUnit > scoreBest) {
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
                if (thisUnit > scoreBest) {
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
                if (thisUnit > scoreBest) {
                    scoreBest = thisUnit;
                    unit.setTarget(&target);
                }
            }

            // If unit is close, add this unit to the targeted by vector
            if (unit.hasTarget() && unit.isWithinReach(unit.getTarget()) && unit.getRole() == Role::Combat)
                unit.getTarget().getTargetedBy().push_back(unit.weak_from_this());
        }

        void getEngagePosition(UnitInfo& unit)
        {
            if (!unit.hasTarget()) {
                unit.setEngagePosition(Positions::Invalid);
                return;
            }

            if (unit.getRole() == Role::Defender || unit.isSuicidal()) {
                unit.setEngagePosition(unit.getTarget().getPosition());
                return;
            }

            // Have to set it to at least 64 or units can't path sometimes to engage position
            auto range = max(64.0, unit.getTarget().getType().isFlyer() ? unit.getAirRange() : unit.getGroundRange());
            if (unit.getTargetPath().isReachable() && (!unit.isWithinRange(unit.getTarget()) || unit.unit()->isLoaded())) {
                auto usedType = BWEB::Map::isUsed(TilePosition(unit.getTarget().getTilePosition()));

                auto engagePosition = Util::findPointOnPath(unit.getTargetPath(), [&](Position p) {
                    auto usedHere = BWEB::Map::isUsed(TilePosition(p));
                    return usedHere == UnitTypes::None && Util::boxDistance(unit.getTarget().getType(), unit.getTarget().getPosition(), unit.getType(), p) <= range;
                });

                if (engagePosition.isValid()) {
                    unit.setEngagePosition(engagePosition);
                    return;
                }
            }

            auto distance = Util::boxDistance(unit.getType(), unit.getPosition(), unit.getTarget().getType(), unit.getTarget().getPosition());
            auto direction = ((distance - range) / distance);
            auto engageX = int((unit.getPosition().x - unit.getTarget().getPosition().x) * direction);
            auto engageY = int((unit.getPosition().y - unit.getTarget().getPosition().y) * direction);
            auto engagePosition = unit.getPosition() - Position(engageX, engageY);

            // If unit is loaded or further than their range, we want to calculate the expected engage position
            if (distance > range || unit.unit()->isLoaded())
                unit.setEngagePosition(engagePosition);
            else
                unit.setEngagePosition(unit.getPosition());
        }

        void getEngageDistance(UnitInfo& unit)
        {
            if (unit.getRole() == Role::Defender && unit.hasTarget()) {
                auto range = unit.getTarget().isFlying() ? unit.getAirRange() : unit.getGroundRange();
                unit.setEngDist(Util::boxDistance(unit.getType(), unit.getPosition(), unit.getTarget().getType(), unit.getTarget().getPosition()) - range);
                return;
            }

            if (!unit.hasTarget() || (unit.getPlayer() == Broodwar->self() && unit.getRole() != Role::Combat)) {
                unit.setEngDist(0.0);
                return;
            }

            if (unit.getPlayer() == Broodwar->self() && !Terrain::isInAllyTerritory(unit.getTilePosition()) && !unit.getTargetPath().isReachable() && !unit.getTarget().getType().isBuilding() && !unit.isFlying() && !unit.getTarget().isFlying() && Grids::getMobility(unit.getTarget().getPosition()) >= 4) {
                unit.setEngDist(DBL_MAX);
                return;
            }

            auto dist = (unit.isFlying() || unit.getTarget().isFlying()) ? unit.getPosition().getDistance(unit.getEngagePosition()) : BWEB::Map::getGroundDistance(unit.getPosition(), unit.getEngagePosition()) - 32.0;
            if (dist == DBL_MAX)
                dist = 2.0 * unit.getPosition().getDistance(unit.getEngagePosition());

            unit.setEngDist(dist);
        }

        void getPathToTarget(UnitInfo& unit)
        {
            // If unnecessary to get path
            if (unit.getRole() != Role::Combat)
                return;

            // If no target, no distance/path available
            if (!unit.hasTarget()) {
                BWEB::Path newPath(unit.getPosition(), unit.getDestination(), unit.getType());
                unit.setEngDist(0.0);
                unit.setTargetPath(newPath);
                return;
            }

            // Don't generate a target path in these cases
            if (unit.getTarget().isFlying() || unit.isFlying()) {
                BWEB::Path newPath(unit.getPosition(), unit.getDestination(), unit.getType());
                unit.setTargetPath(newPath);
                return;
            }

            // See if we can borrow a path from a targeter around us
            auto maxDist = Players::getSupply(PlayerState::Self, Races::None) / 2;
            for (auto &t : unit.getTarget().getTargetedBy()) {
                if (auto targeter = t.lock()) {
                    if (unit.getTarget() == targeter->getTarget() && targeter->getTargetPath().isReachable() && targeter->getPosition().getDistance(targeter->getTarget().getPosition()) < unit.getPosition().getDistance(unit.getTarget().getPosition()) && targeter->getPosition().getDistance(unit.getPosition()) < maxDist) {
                        unit.setTargetPath(targeter->getTargetPath());
                        return;
                    }
                }
            }
            
            // TODO: Need a custom target walkable for a building
            auto targetType = unit.getTarget().getType();
            auto targetPos = unit.getTarget().getPosition();
            const auto targetWalkable = [&](const TilePosition &t) {
                return targetType.isBuilding() && Util::rectangleIntersect(targetPos, targetPos + Position(targetType.tileSize()), Position(t));
            };

            // If should create path, grab one from BWEB
            if (unit.getTargetPath().getTarget() != TilePosition(unit.getTarget().getPosition()) && (!mapBWEM.GetArea(TilePosition(unit.getPosition())) || !mapBWEM.GetArea(TilePosition(unit.getTarget().getPosition())) || mapBWEM.GetArea(TilePosition(unit.getPosition()))->AccessibleFrom(mapBWEM.GetArea(TilePosition(unit.getTarget().getPosition()))))) {
                BWEB::Path newPath(unit.getPosition(), unit.getTarget().getPosition(), unit.getType());
                newPath.generateJPS([&](const TilePosition &t) { return newPath.unitWalkable(t) || targetWalkable(t); });
                unit.setTargetPath(newPath);

                if (!newPath.isReachable() && newPath.unitWalkable(unit.getTilePosition()))
                    unit.getTarget().lastUnreachableFrame = Broodwar->getFrameCount();                
            }
        }
    }

    void getTarget(UnitInfo& unit)
    {
        auto pState = unit.targetsFriendly() ? PlayerState::Self : PlayerState::Enemy;

        if (unit.getRole() == Role::Combat || unit.getRole() == Role::Support || unit.getRole() == Role::Defender || unit.getRole() == Role::Worker || unit.getRole() == Role::Scout) {
            getBestTarget(unit, pState);
            getSimTarget(unit, PlayerState::Enemy);
            getPathToTarget(unit);
            getEngagePosition(unit);
            getEngageDistance(unit);
        }
    }
}