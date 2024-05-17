#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Command
{
    namespace {
        set<Position> testPositions;
    }

    bool click(UnitInfo& unit)
    {
        // Shield Battery - Repair Shields
        if (com(Protoss_Shield_Battery) > 0 && (unit.unit()->getGroundWeaponCooldown() > Broodwar->getLatencyFrames() || unit.unit()->getAirWeaponCooldown() > Broodwar->getLatencyFrames()) && unit.getType().maxShields() > 0 && (unit.unit()->getShields() <= 10 || (unit.unit()->getShields() < unit.getType().maxShields() && unit.unit()->getOrderTarget() && unit.unit()->getOrderTarget()->exists() && unit.unit()->getOrderTarget()->getType() == Protoss_Shield_Battery && unit.unit()->getOrderTarget()->getEnergy() >= 10))) {
            auto battery = Util::getClosestUnit(unit.getPosition(), PlayerState::Self, [&](auto &u) {
                return u->getType() == Protoss_Shield_Battery && u->unit()->isCompleted() && u->getEnergy() > 10;
            });

            if (battery && ((unit.getType().isFlyer() && (!unit.hasTarget() || (unit.getTarget().lock()->getPosition().getDistance(unit.getPosition()) >= 320))) || unit.unit()->getDistance(battery->getPosition()) < 320)) {
                if (unit.unit()->getLastCommand().getType() != UnitCommandTypes::Right_Click_Unit || unit.unit()->getLastCommand().getTarget() != battery->unit())
                    unit.unit()->rightClick(battery->unit());
                return true;
            }
        }


        // Bunker - Loading / Unloading
        else if (unit.getType() == Terran_Marine && com(Terran_Bunker) > 0) {

            auto bunker = Util::getClosestUnit(unit.getPosition(), PlayerState::Self, [&](auto &u) {
                return (u->getType() == Terran_Bunker && u->unit()->getSpaceRemaining() > 0);
            });

            if (bunker) {
                unit.unit()->rightClick(bunker->unit());
                return true;
            }

            if (bunker && bunker->unit() && unit.hasTarget()) {
                if (unit.getTarget().lock()->unit()->exists() && unit.getTarget().lock()->getPosition().getDistance(unit.getPosition()) <= 320) {
                    unit.unit()->rightClick(bunker->unit());
                    return true;
                }
                if (unit.unit()->isLoaded() && unit.getTarget().lock()->getPosition().getDistance(unit.getPosition()) > 320)
                    bunker->unit()->unloadAll();
            }
        }
        return false;
    }

    bool siege(UnitInfo& unit)
    {
        auto targetDist = unit.hasTarget() ? unit.getPosition().getDistance(unit.getTarget().lock()->getPosition()) : 0.0;

        // Siege Tanks - Siege
        if (unit.getType() == Terran_Siege_Tank_Tank_Mode) {
            if (unit.hasTarget() && unit.getTarget().lock()->getGroundRange() > 32.0 && targetDist <= 450.0 && targetDist >= 100.0 && unit.getLocalState() == LocalState::Attack)
                unit.unit()->siege();
            if (unit.getGlobalState() == GlobalState::Retreat && unit.getPosition().getDistance(Terrain::getDefendPosition()) < 320)
                unit.unit()->siege();
        }

        // Siege Tanks - Unsiege
        else if (unit.getType() == Terran_Siege_Tank_Siege_Mode) {
            if (unit.hasTarget() && (unit.getTarget().lock()->getGroundRange() <= 32.0 || targetDist < 100.0 || targetDist > 450.0 || unit.getLocalState() == LocalState::Retreat)) {
                if (unit.unit()->getLastCommand().getType() != UnitCommandTypes::Unsiege)
                    unit.unit()->unsiege();
                return true;
            }
        }
        return false;
    }

    bool repair(UnitInfo& unit)
    {
        // SCV
        if (unit.getType() == Terran_SCV) {

            //// Repair closest injured mech
            //auto &mech = Util::getClosestUnit(unit.getPosition(), PlayerState::Self, [&](auto &u) {
            //    return (u.getType().isMechanical() && u.getPercentHealth() < 1.0);
            //});

            //if (!Spy::enemyRush() && mech && mech->unit() && unit.getPosition().getDistance(mech->getPosition()) <= 320 && Grids::getMobility(mech->getWalkPosition()) > 0) {
            //    if (!unit.unit()->isRepairing() || unit.unit()->getLastCommand().getType() != UnitCommandTypes::Repair || unit.unit()->getLastCommand().getTarget() != mech->unit())
            //        unit.unit()->repair(mech->unit());
            //    return true;
            //}

            //// Repair closest injured building
            //auto &building = Util::getClosestUnit(unit.getPosition(), PlayerState::Self, [&](auto &u) {
            //    return u.getPercentHealth() < 0.35 || (u.getType() == Terran_Bunker && u.getPercentHealth() < 1.0);
            //});

            //if (building && building->unit() && (!Spy::enemyRush() || building->getType() == Terran_Bunker)) {
            //    if (!unit.unit()->isRepairing() || unit.unit()->getLastCommand().getType() != UnitCommandTypes::Repair || unit.unit()->getLastCommand().getTarget() != building->unit())
            //        unit.unit()->repair(building->unit());
            //    return true;
            //}
        }
        return false;
    }

    bool burrow(UnitInfo& unit)
    {
        // Vulture spider mine burrowing
        if (unit.getType() == Terran_Vulture) {
            if (Broodwar->self()->hasResearched(TechTypes::Spider_Mines) && unit.unit()->getSpiderMineCount() > 0 && unit.hasSimTarget() && unit.getPosition().getDistance(unit.getSimTarget().lock()->getPosition()) <= 400 && Broodwar->getUnitsInRadius(unit.getPosition(), 128, Filter::GetType == Terran_Vulture_Spider_Mine).size() <= 3) {
                if (unit.unit()->getLastCommand().getTechType() != TechTypes::Spider_Mines || unit.unit()->getLastCommand().getTargetPosition().getDistance(unit.getPosition()) > 8)
                    unit.unit()->useTech(TechTypes::Spider_Mines, unit.getPosition());
                return true;
            }
        }

        // Lurker burrowing
        else if (unit.getType() == Zerg_Lurker) {
            if (!unit.unit()->isBurrowed() && unit.getFormation().isValid() && unit.getPosition().getDistance(unit.getFormation()) < 64.0) {
                unit.unit()->burrow();
                return true;
            }
            else if (!unit.unit()->isBurrowed() && unit.getLocalState() == LocalState::Attack && unit.getPosition().getDistance(unit.getEngagePosition()) < 16.0) {
                unit.unit()->burrow();
                return true;
            }
            else if (unit.unit()->isBurrowed() && unit.getPosition().getDistance(unit.getEngagePosition()) > 32.0) {
                unit.unit()->unburrow();
                return true;
            }
        }

        // Drone
        else if (unit.getType().isWorker() && Broodwar->self()->hasResearched(TechTypes::Burrowing)) {
            const auto resourceThreatened = (unit.hasResource() && Grids::getEGroundThreat(unit.getResource().lock()->getPosition()) > 0.0f) || !unit.getUnitsTargetingThis().empty();
            const auto threatened = unit.hasResource() && resourceThreatened && unit.hasTarget() && unit.getTarget().lock()->isThreatening() && !unit.getTarget().lock()->isFlying() && !unit.getTarget().lock()->getType().isWorker();

            if (!unit.isBurrowed()) {
                if (threatened) {
                    unit.unit()->burrow();
                    return true;
                }
            }
            else if (unit.isBurrowed()) {
                if (!threatened) {
                    unit.unit()->unburrow();
                    return true;
                }
            }
        }

        // Zergling burrowing
        else if (unit.getType() == Zerg_Zergling && Broodwar->self()->hasResearched(TechTypes::Burrowing)) {
            auto fullHealth = unit.getHealth() == unit.getType().maxHitPoints();

            if (!unit.isBurrowed() && unit.getGoalType() == GoalType::Contain && !Planning::overlapsPlan(unit, unit.getPosition()) && unit.getGoal().getDistance(unit.getPosition()) < 16.0 && !Actions::overlapsDetection(unit.unit(), unit.getPosition(), PlayerState::Enemy))
                unit.unit()->burrow();
            if (unit.isBurrowed() && (unit.getGoalType() != GoalType::Contain || Planning::overlapsPlan(unit, unit.getPosition()) || unit.getGoal().getDistance(unit.getPosition()) > 16.0 || Actions::overlapsDetection(unit.unit(), unit.getPosition(), PlayerState::Enemy)))
                unit.unit()->unburrow();
        }

        return false;
    }

    bool cast(UnitInfo& unit)
    {
        // Battlecruiser - Yamato
        if (unit.getType() == Terran_Battlecruiser) {
            if ((unit.unit()->getOrder() == Orders::FireYamatoGun || (Broodwar->self()->hasResearched(TechTypes::Yamato_Gun) && unit.getEnergy() >= TechTypes::Yamato_Gun.energyCost()) && unit.getTarget().lock()->unit()->getHitPoints() >= 80) && unit.hasTarget() && unit.getTarget().lock()->unit()->exists()) {
                if ((unit.unit()->getLastCommand().getType() != UnitCommandTypes::Use_Tech || unit.unit()->getLastCommand().getTarget() != unit.getTarget().lock()->unit()))
                    unit.unit()->useTech(TechTypes::Yamato_Gun, unit.getTarget().lock()->unit());

                Actions::addAction(unit.unit(), unit.getTarget().lock()->getPosition(), TechTypes::Yamato_Gun, PlayerState::Self);
                return true;
            }
        }

        // Ghost - Cloak / Nuke
        else if (unit.getType() == Terran_Ghost) {

            if (!unit.unit()->isCloaked() && unit.getEnergy() >= 50 && unit.getPosition().getDistance(unit.getEngagePosition()) < 320 && !Actions::overlapsDetection(unit.unit(), unit.getEngagePosition(), PlayerState::Enemy))
                unit.unit()->useTech(TechTypes::Personnel_Cloaking);

            if (com(Terran_Nuclear_Missile) > 0 && unit.hasTarget() && unit.getTarget().lock()->getWalkPosition().isValid() && unit.unit()->isCloaked() && Grids::getEAirCluster(unit.getTarget().lock()->getWalkPosition()) + Grids::getEGroundCluster(unit.getTarget().lock()->getWalkPosition()) > 5.0 && unit.getPosition().getDistance(unit.getTarget().lock()->getPosition()) <= 320 && unit.getPosition().getDistance(unit.getTarget().lock()->getPosition()) > 200) {
                if (unit.unit()->getLastCommand().getType() != UnitCommandTypes::Use_Tech_Unit || unit.unit()->getLastCommand().getTarget() != unit.getTarget().lock()->unit()) {
                    unit.unit()->useTech(TechTypes::Nuclear_Strike, unit.getTarget().lock()->unit());
                    Actions::addAction(unit.unit(), unit.getTarget().lock()->getPosition(), TechTypes::Nuclear_Strike, PlayerState::Self);
                    return true;
                }
            }
            if (unit.unit()->getOrder() == Orders::NukePaint || unit.unit()->getOrder() == Orders::NukeTrack || unit.unit()->getOrder() == Orders::CastNuclearStrike) {
                Actions::addAction(unit.unit(), unit.unit()->getOrderTargetPosition(), TechTypes::Nuclear_Strike, PlayerState::Self);
                return true;
            }
        }

        // Marine / Firebat - Stim Packs
        else if (Broodwar->self()->hasResearched(TechTypes::Stim_Packs) && (unit.getType() == Terran_Marine || unit.getType() == Terran_Firebat) && !unit.unit()->isStimmed() && unit.hasTarget() && unit.getTarget().lock()->getPosition().isValid() && unit.unit()->getDistance(unit.getTarget().lock()->getPosition()) <= unit.getGroundRange())
            unit.unit()->useTech(TechTypes::Stim_Packs);

        // Science Vessel - Defensive Matrix
        else if (unit.getType() == Terran_Science_Vessel && unit.getEnergy() >= TechTypes::Defensive_Matrix) {
            auto ally = Util::getClosestUnit(unit.getPosition(), PlayerState::Self, [&](auto &u) {
                return (u->unit()->isUnderAttack());
            });
            if (ally && ally->getPosition().getDistance(unit.getPosition()) < 640)
                unit.unit()->useTech(TechTypes::Defensive_Matrix, ally->unit());
            return true;
        }

        // Scanner
        else if (unit.getType() == Spell_Scanner_Sweep)
            Actions::addAction(unit.unit(), unit.getPosition(), Spell_Scanner_Sweep, PlayerState::Self);

        // Wraith - Cloak
        else if (unit.getType() == Terran_Wraith) {
            if (unit.getHealth() >= 120 && !unit.unit()->isCloaked() && unit.getEnergy() >= 50 && unit.getPosition().getDistance(unit.getEngagePosition()) < 320 && !Actions::overlapsDetection(unit.unit(), unit.getEngagePosition(), PlayerState::Enemy))
                unit.unit()->useTech(TechTypes::Cloaking_Field);
            else if (unit.getHealth() <= 90 && unit.unit()->isCloaked())
                unit.unit()->useTech(TechTypes::Cloaking_Field);
        }

        // Arbiters - Stasis Field       
        else if (unit.getType() == Protoss_Arbiter) {

            // If close to target and can cast Stasis Field
            if (unit.hasTarget() && unit.canStartCast(TechTypes::Stasis_Field, unit.getTarget().lock()->getPosition())) {
                unit.unit()->useTech(TechTypes::Stasis_Field, unit.getTarget().lock()->unit());
                Actions::addAction(unit.unit(), unit.getTarget().lock()->getPosition(), TechTypes::Stasis_Field, PlayerState::Self);
                return true;
            }
        }

        // Corsair - Disruption Web
        else if (unit.getType() == Protoss_Corsair) {

            // If close to a slow enemy and can cast disruption web
            if (unit.getEnergy() >= TechTypes::Disruption_Web.energyCost() && Broodwar->self()->hasResearched(TechTypes::Disruption_Web)) {
                auto slowEnemy = Util::getClosestUnit(unit.getPosition(), PlayerState::Enemy, [&](auto &u) {
                    return u->getPosition().getDistance(unit.getPosition()) < 256.0 && !Actions::overlapsActions(unit.unit(), u->getPosition(), TechTypes::Disruption_Web, PlayerState::Self, 96) && u->hasAttackedRecently() && u->getSpeed() <= Protoss_Reaver.topSpeed();
                });

                if (slowEnemy) {
                    Actions::addAction(unit.unit(), slowEnemy->getPosition(), TechTypes::Disruption_Web, PlayerState::Self);
                    unit.unit()->useTech(TechTypes::Disruption_Web, slowEnemy->getPosition());
                    return true;
                }
            }
        }

        // High Templar - Psi Storm
        else if (unit.getType() == Protoss_High_Templar) {

            // If close to target and can cast Psi Storm
            if (unit.hasTarget() && unit.getPosition().getDistance(unit.getTarget().lock()->getPosition()) <= 320 && unit.canStartCast(TechTypes::Psionic_Storm, unit.getTarget().lock()->getPosition())) {
                unit.unit()->useTech(TechTypes::Psionic_Storm, unit.getTarget().lock()->getPosition());
                Actions::addAction(unit.unit(), unit.getTarget().lock()->getPosition(), TechTypes::Psionic_Storm, PlayerState::Neutral);
                return true;
            }
        }

        // Defiler - Consume / Dark Swarm / Plague
        else if (unit.getType() == Zerg_Defiler && !unit.isBurrowed()) {

            // If close to target and can cast Plague
            if (unit.hasTarget() && unit.getLocalState() != LocalState::None && unit.canStartCast(TechTypes::Plague, unit.getTarget().lock()->getPosition())) {
                //if (unit.unit()->getLastCommand().getTechType() != TechTypes::Plague)
                unit.unit()->useTech(TechTypes::Plague, unit.getTarget().lock()->getPosition());
                Actions::addAction(unit.unit(), unit.getTarget().lock()->getPosition(), TechTypes::Plague, PlayerState::Neutral);
                return true;
            }

            // If close to target and can cast Dark Swarm
            if (unit.hasTarget() && Players::ZvT() && unit.getPosition().getDistance(unit.getTarget().lock()->getPosition()) <= 400 && unit.canStartCast(TechTypes::Dark_Swarm, unit.getTarget().lock()->getPosition())) {
                //if (unit.unit()->getLastCommand().getTechType() != TechTypes::Dark_Swarm)
                unit.unit()->useTech(TechTypes::Dark_Swarm, unit.getTarget().lock()->getPosition());
                Actions::addAction(unit.unit(), unit.getTarget().lock()->getPosition(), TechTypes::Dark_Swarm, PlayerState::Neutral);
                return true;
            }

            // If within range of an intermediate point within engaging distance
            if (unit.hasTarget() && Players::ZvT()) {
                for (auto &tile : unit.getDestinationPath().getTiles()) {
                    auto center = Position(tile) + Position(16, 16);

                    auto distMin = BuildOrder::getCompositionPercentage(Zerg_Hydralisk) > 0.0 ? 200.0 : 0.0;
                    auto dist = center.getDistance(unit.getTarget().lock()->getPosition());

                    if (dist <= 400 && dist > distMin && unit.canStartCast(TechTypes::Dark_Swarm, center)) {
                        if (unit.unit()->getLastCommand().getTechType() != TechTypes::Dark_Swarm)
                            unit.unit()->useTech(TechTypes::Dark_Swarm, center);
                        Actions::addAction(unit.unit(), center, TechTypes::Dark_Swarm, PlayerState::Neutral);
                        return true;
                    }
                }
            }

            // If close to target and need to Consume
            if (unit.hasTarget() && unit.getEnergy() < 150 && unit.getPosition().getDistance(unit.getTarget().lock()->getPosition()) <= 160.0 && unit.canStartCast(TechTypes::Consume, unit.getTarget().lock()->getPosition())) {
                unit.unit()->useTech(TechTypes::Consume, unit.getTarget().lock()->unit());
                return true;
            }
        }
        return false;
    }

    bool morph(UnitInfo& unit)
    {
        auto canAffordMorph = [&](UnitType type) {
            if (Broodwar->self()->minerals() >= type.mineralPrice() && Broodwar->self()->gas() >= type.gasPrice() && Broodwar->self()->supplyTotal() - Broodwar->self()->supplyUsed() > type.supplyRequired())
                return true;
            return false;
        };

        // High Templar - Archon Morph
        if (unit.getType() == Protoss_High_Templar) {
            auto lowEnergyThreat = unit.getEnergy() < TechTypes::Psionic_Storm.energyCost() && Grids::getEGroundThreat(unit.getWalkPosition()) > 0.0f;
            auto wantArchons = vis(Protoss_Archon) / BuildOrder::getCompositionPercentage(Protoss_Archon) < vis(Protoss_High_Templar) / BuildOrder::getCompositionPercentage(Protoss_High_Templar);

            if (!Players::vT() && (lowEnergyThreat || wantArchons)) {

                // Try to find a friendly templar who is low energy and is threatened
                auto templar = Util::getClosestUnit(unit.getPosition(), PlayerState::Self, [&](auto &u) {
                    return *u != unit && u->getType() == Protoss_High_Templar && u->unit()->isCompleted() && (wantArchons || (u->getEnergy() < 75 && Grids::getEGroundThreat(u->getWalkPosition()) > 0.0));
                });

                if (templar) {
                    if (unit.unit()->getLastCommand().getType() != UnitCommandTypes::Morph)
                        unit.unit()->useTech(TechTypes::Archon_Warp, templar->unit());
                    return true;
                }
            }
        }

        // Hydralisk - Lurker Morph
        else if (unit.getType() == Zerg_Hydralisk && Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect)) {
            const auto wantLurkers = (vis(Zerg_Lurker) + vis(Zerg_Lurker_Egg)) / BuildOrder::getCompositionPercentage(Zerg_Lurker) < vis(Zerg_Hydralisk) / BuildOrder::getCompositionPercentage(Zerg_Hydralisk);
            const auto onlyLurkers = BuildOrder::getCompositionPercentage(Zerg_Lurker) >= 1.00 || (Players::ZvT() && BuildOrder::getCompositionPercentage(Zerg_Lurker) == 0.0 && BuildOrder::getCompositionPercentage(Zerg_Hydralisk) == 0.0);

            if ((wantLurkers || onlyLurkers)) {
                const auto furthestHydra = Util::getFurthestUnit(Terrain::getEnemyStartingPosition(), PlayerState::Self, [&](auto &u) {
                    return u->getType() == UnitTypes::Zerg_Hydralisk;
                });
                if (canAffordMorph(Zerg_Lurker) && furthestHydra && unit == *furthestHydra) {
                    if (unit.unit()->getLastCommand().getType() != UnitCommandTypes::Morph)
                        unit.unit()->morph(Zerg_Lurker);
                    return true;
                }
            }
        }

        // Mutalisk - Devourer / Guardian Morph
        else if (unit.getType() == Zerg_Mutalisk && com(Zerg_Greater_Spire) > 0) {
            const auto wantDevo = vis(Zerg_Devourer) / BuildOrder::getCompositionPercentage(Zerg_Devourer) < (vis(Zerg_Mutalisk) / BuildOrder::getCompositionPercentage(Zerg_Mutalisk)) + (vis(Zerg_Guardian) / BuildOrder::getCompositionPercentage(Zerg_Guardian));
            const auto wantGuard = vis(Zerg_Guardian) / BuildOrder::getCompositionPercentage(Zerg_Guardian) < (vis(Zerg_Mutalisk) / BuildOrder::getCompositionPercentage(Zerg_Mutalisk)) + (vis(Zerg_Devourer) / BuildOrder::getCompositionPercentage(Zerg_Devourer));

            const auto onlyDevo = BuildOrder::getCompositionPercentage(Zerg_Devourer) >= 1.00;
            const auto onlyGuard = BuildOrder::getCompositionPercentage(Zerg_Guardian) >= 1.00;

            if ((onlyGuard || wantGuard)) {
                if (canAffordMorph(Zerg_Guardian) && unit.getPosition().getDistance(unit.getSimTarget().lock()->getPosition()) >= unit.getEngageRadius() && unit.getPosition().getDistance(unit.getSimTarget().lock()->getPosition()) < unit.getEngageRadius() + 160.0) {
                    if (unit.unit()->getLastCommand().getType() != UnitCommandTypes::Morph)
                        unit.unit()->morph(Zerg_Guardian);
                    return true;
                }
            }
            else if ((onlyDevo || wantDevo)) {
                if (canAffordMorph(Zerg_Devourer) && unit.getPosition().getDistance(unit.getSimTarget().lock()->getPosition()) >= unit.getEngageRadius()) {
                    if (unit.unit()->getLastCommand().getType() != UnitCommandTypes::Morph)
                        unit.unit()->morph(Zerg_Devourer);
                    return true;
                }
            }
        }
        return false;
    }

    bool train(UnitInfo& unit)
    {
        // Carrier - Train Interceptor
        if (unit.getType() == Protoss_Carrier && unit.unit()->getInterceptorCount() < (4 + (Broodwar->self()->getUpgradeLevel(UpgradeTypes::Carrier_Capacity) * 4))) {
            unit.unit()->train(Protoss_Interceptor);
            return false;
        }
        // Reaver - Train Scarab
        if (unit.getType() == Protoss_Reaver && unit.unit()->getScarabCount() < (5 + (Broodwar->self()->getUpgradeLevel(UpgradeTypes::Reaver_Capacity) * 5))) {
            unit.unit()->train(Protoss_Scarab);
            return false;
        }
        return false;
    }

    bool returnResource(UnitInfo& unit)
    {
        // Can't return cargo if we aren't carrying a resource or overlapping a building position
        if ((!unit.unit()->isCarryingGas() && !unit.unit()->isCarryingMinerals()))
            return false;

        auto checkPath = (unit.hasResource() && unit.getPosition().getDistance(unit.getResource().lock()->getPosition()) > 320.0) || (!unit.hasResource() && !Terrain::inTerritory(PlayerState::Self, unit.getPosition()));
        if (checkPath) {
            // TODO: Create a path to the closest station and check if it's safe
        }

        //auto hatch = Util::getClosestUnit(unit.getPosition(), PlayerState::Self, [&](auto &u) {
        //    return u.getType() == Zerg_Hatchery;
        //});
        //if (hatch && unit.hasResource()) {
        //    auto distHome = Util::boxDistance(hatch->getType(), hatch->getPosition(), unit.getType(), unit.getPosition());
        //    if (distHome <= 0) {
        //        auto frame = Broodwar->getFrameCount() - Broodwar->getLatencyFrames() - 11;
        //        auto optimal = unit.getPositionHistory().find(frame);
        //        if (optimal != unit.getPositionHistory().end())
        //            unit.getResource().getReturnOrderPositions().insert(optimal->second);
        //        unit.getPositionHistory().clear();
        //    }

        //    if (unit.getResource().getReturnOrderPositions().find(unit.getPosition()) != unit.getResource().getReturnOrderPositions().end() && Broodwar->getFrameCount() - unit.unit()->getLastCommandFrame() > 24.0) {
        //        unit.unit()->returnCargo();
        //        return true;
        //    }
        //}

        // TODO: Check if we have a building to place first?
        if ((unit.unit()->isCarryingMinerals() || unit.unit()->isCarryingGas())) {
            if ((unit.unit()->isIdle() || (unit.unit()->getOrder() != Orders::ReturnMinerals && unit.unit()->getOrder() != Orders::ReturnGas)) && unit.unit()->getLastCommand().getType() != UnitCommandTypes::Return_Cargo)
                unit.unit()->returnCargo();
            return true;
        }

        return false;
    }

    bool clearNeutral(UnitInfo& unit)
    {
        // For now only workers clear neutrals
        auto resourceDepot = Broodwar->self()->getRace().getResourceDepot();
        if (!unit.getType().isWorker()
            || Util::getTime() < Time(6, 0)
            || Stations::getStations(PlayerState::Self).size() <= 2
            || (BuildOrder::buildCount(resourceDepot) == vis(resourceDepot) && BuildOrder::isOpener())
            || unit.unit()->isCarryingMinerals()
            || unit.unit()->isCarryingGas())
            return false;

        // Find boulders to clear
        for (auto &b : Resources::getMyBoulders()) {
            ResourceInfo &boulder = *b;
            if (!boulder.unit() || !boulder.unit()->exists())
                continue;
            if ((unit.getPosition().getDistance(boulder.getPosition()) <= 320.0 && boulder.getGathererCount() == 0) || (unit.unit()->isGatheringMinerals() && unit.unit()->getOrderTarget() == boulder.unit())) {

                auto closestWorker = Util::getClosestUnit(boulder.getPosition(), PlayerState::Self, [&](auto &u) {
                    return u->getRole() == Role::Worker;
                });

                if (closestWorker && *closestWorker != unit)
                    continue;

                if (unit.unit()->getOrderTarget() != boulder.unit())
                    unit.unit()->gather(boulder.unit());
                return true;
            }
        }
        return false;
    }

    bool build(UnitInfo& unit)
    {
        if (unit.getRole() != Role::Worker || !unit.getBuildPosition().isValid())
            return false;

        const auto fullyVisible = Broodwar->isVisible(unit.getBuildPosition())
            && Broodwar->isVisible(unit.getBuildPosition() + TilePosition(unit.getBuildType().tileWidth(), 0))
            && Broodwar->isVisible(unit.getBuildPosition() + TilePosition(unit.getBuildType().tileWidth(), unit.getBuildType().tileHeight()))
            && Broodwar->isVisible(unit.getBuildPosition() + TilePosition(0, unit.getBuildType().tileHeight()));

        const auto canAfford = Broodwar->self()->minerals() >= unit.getBuildType().mineralPrice() && Broodwar->self()->gas() >= unit.getBuildType().gasPrice();

        // If build position is fully visible and unit is close to it, start building as soon as possible
        if (fullyVisible && canAfford && unit.isWithinBuildRange()) {
            if (unit.unit()->getLastCommandFrame() < Broodwar->getFrameCount() - 8)
                unit.unit()->build(unit.getBuildType(), unit.getBuildPosition());
            return true;
        }
        return false;
    }

    bool gather(UnitInfo& unit)
    {
        if (unit.getRole() != Role::Worker)
            return false;

        const auto hasMineableResource = unit.hasResource() && (unit.getResource().lock()->getResourceState() == ResourceState::Mineable || Util::getTime() < Time(4, 00)) && unit.getResource().lock()->unit()->exists();

        const auto canGather = [&](Unit resource) {
            if (unit.unit()->getTarget() == resource)
                return false;

            auto closestChokepoint = Util::getClosestChokepoint(unit.getPosition());
            auto nearNonBlockingChoke = closestChokepoint && !closestChokepoint->Blocked() && unit.getPosition().getDistance(Position(closestChokepoint->Center())) < 160.0;
            auto boxDist = Util::boxDistance(unit.getType(), unit.getPosition(), resource->getType(), resource->getPosition());

            if (Grids::getEGroundThreat(unit.getPosition()) > 0.0f)
                return true;
            if (hasMineableResource && (unit.isWithinGatherRange() || Grids::getAGroundCluster(unit.getPosition()) > 0.0f || nearNonBlockingChoke))
                return true;
            if (!hasMineableResource && resource->exists())
                return true;
            if (Planning::overlapsPlan(unit, unit.getPosition()))
                return true;
            if ((unit.hasResource() && boxDist > 0 && unit.unit()->getOrder() == Orders::MoveToMinerals && unit.getResource().lock()->getGatherOrderPositions().find(unit.getPosition()) != unit.getResource().lock()->getGatherOrderPositions().end()))
                return true;
            return false;
        };

        // Attempt to mineral walk towards building location if we have one
        if (unit.getBuildPosition().isValid()) {
            auto buildCenter = Position(unit.getBuildPosition()) + Position(unit.getBuildType().tileWidth() * 16, unit.getBuildType().tileHeight() * 16);
            if (unit.getPosition().getDistance(buildCenter) > 256.0) {
                auto closestMineral = Util::getClosestUnit(buildCenter, PlayerState::Neutral, [&](auto &u) {
                    return u->getType().isMineralField();
                });
                if (closestMineral && closestMineral->getPosition().getDistance(buildCenter) < unit.getPosition().getDistance(buildCenter) && closestMineral->getPosition().getDistance(buildCenter) < 256.0) {
                    unit.unit()->gather(closestMineral->unit());
                    return true;
                }
            }
        }

        // These worker order timers are based off Stardust: https://github.com/bmnielsen/Stardust/blob/master/src/Workers/WorkerOrderTimer.cpp#L153
        // From what Bruce found, you can prevent what seems like ~7 frames of a worker "waiting" to mine
        if (unit.hasResource() && Util::getTime() < Time(1, 30)) {
            auto boxDist = Util::boxDistance(unit.getType(), unit.getPosition(), unit.getResource().lock()->getType(), unit.getResource().lock()->getPosition());
            if (boxDist == 0) {
                auto frame = Broodwar->getFrameCount() - Broodwar->getLatencyFrames() - 11;
                auto optimal = unit.getPositionHistory().find(frame);
                if (optimal != unit.getPositionHistory().end()) {

                    // Must check if we found a position to close, otherwise sometimes we send gather commands every frame or issue the command too early, not benefting from our optimization
                    for (auto &[frame, position] : unit.getPositionHistory()) {
                        if (frame <= (frame + 2))
                            continue;
                        unit.getResource().lock()->getGatherOrderPositions().erase(position);
                    }
                    unit.getResource().lock()->getGatherOrderPositions().insert(optimal->second);
                }
                unit.getPositionHistory().clear();
            }
        }

        // Check if we're trying to build a structure near this worker
        if (unit.hasResource()) {
            auto station = unit.getResource().lock()->getStation();

            if (station) {
                auto builder = Util::getClosestUnit(unit.getResource().lock()->getPosition(), PlayerState::Self, [&](auto &u) {
                    return *u != unit && u->getBuildType() != UnitTypes::None && station->getDefenses().find(u->getBuildPosition()) != station->getDefenses().end();
                });

                // Builder is close and may need space opened up
                if (builder) {
                    auto center = Position(builder->getBuildPosition()) + Position(32, 32);
                    if (builder->getPosition().getDistance(center) < 64.0 && unit.getResource().lock()->getPosition().getDistance(center) < 160.0 && Broodwar->self()->minerals() >= builder->getBuildType().mineralPrice() && Broodwar->self()->gas() >= builder->getBuildType().gasPrice()) {

                        // Get furthest Mineral
                        BWEM::Mineral * furthest = nullptr;
                        auto furthestDist = 0.0;
                        for (auto &resource : unit.getResource().lock()->getStation()->getBase()->Minerals()) {
                            if (resource && resource->Unit()->exists()) {
                                auto dist = resource->Pos().getDistance(center);
                                if (dist > furthestDist) {
                                    furthestDist = dist;
                                    furthest = resource;
                                }
                            }
                        }

                        // Spam gather it to move out of the way
                        if (furthest) {
                            unit.unit()->gather(furthest->Unit());
                            return true;
                        }
                    }
                }

            }
        }

        // Gather from resource
        auto station = Stations::getClosestStationGround(unit.getPosition(), PlayerState::Self);
        auto target = hasMineableResource ? unit.getResource().lock()->unit() : Broodwar->getClosestUnit(station ? station->getResourceCentroid() : BWEB::Map::getMainPosition(), Filter::IsMineralField);
        if (target && canGather(target)) {
            unit.unit()->gather(target);
            return true;
        }
        return false;
    }

    bool special(UnitInfo& unit)
    {
        return click(unit)
            || siege(unit)
            || repair(unit)
            || burrow(unit)
            || cast(unit)
            || morph(unit)
            || train(unit)
            || returnResource(unit)
            || clearNeutral(unit)
            || build(unit)
            || gather(unit);
    }
}