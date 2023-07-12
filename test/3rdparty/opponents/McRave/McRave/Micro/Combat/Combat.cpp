#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Combat {

    namespace {

        int lastRoleChange = 0;
        bool holdChoke = false;

        bool lightUnitNeedsRegroup(UnitInfo& unit)
        {
            if (!unit.isLightAir())
                return false;
            return unit.hasCommander() && unit.getPosition().getDistance(unit.getCommander().lock()->getPosition()) > 64.0;
        }

        void checkHoldChoke()
        {
            auto defensiveAdvantage = (Players::ZvZ() && BuildOrder::getCurrentOpener() == Spy::getEnemyOpener())
                || (Players::ZvZ() && BuildOrder::getCurrentOpener() == "12Pool" && Spy::getEnemyOpener() == "9Pool")
                || (Players::ZvZ() && vis(Zerg_Zergling) >= Players::getVisibleCount(PlayerState::Enemy, Zerg_Zergling) + 2);

            // UMS Setting
            if (Broodwar->getGameType() == BWAPI::GameTypes::Use_Map_Settings) {
                holdChoke = true;
                return;
            }

            // Protoss
            if (Broodwar->self()->getRace() == Races::Protoss && Players::getSupply(PlayerState::Self, Races::None) > 40) {
                holdChoke = BuildOrder::takeNatural()
                    || vis(Protoss_Dragoon) > 0
                    || com(Protoss_Shield_Battery) > 0
                    || BuildOrder::isWallNat()
                    || (BuildOrder::isHideTech() && !Spy::enemyRush())
                    || Players::getSupply(PlayerState::Self, Races::None) > 60
                    || Players::vT();
            }

            // Terran
            if (Broodwar->self()->getRace() == Races::Terran && Players::getSupply(PlayerState::Self, Races::None) > 40)
                holdChoke = true;

            // Zerg
            if (Broodwar->self()->getRace() == Races::Zerg) {
                holdChoke = (!Players::ZvZ() && (Spy::getEnemyBuild() != "2Gate" || !Spy::enemyProxy()))
                    || (defensiveAdvantage && !Spy::enemyPressure() && vis(Zerg_Sunken_Colony) == 0 && com(Zerg_Mutalisk) < 3 && Util::getTime() > Time(3, 30))
                    || (BuildOrder::takeNatural() && total(Zerg_Zergling) >= 10)
                    || Players::getSupply(PlayerState::Self, Races::None) > 60;
            }
        }

        void updateRole(UnitInfo& unit)
        {
            // Can't change role to combat if not a worker or we did one this frame
            if (!unit.getType().isWorker()
                || lastRoleChange == Broodwar->getFrameCount()
                || unit.getBuildType() != None
                || unit.getGoal().isValid()
                || unit.getBuildPosition().isValid())
                return;

            // Only proactively pull the closest worker to our defend position
            auto closestWorker = Util::getClosestUnit(Terrain::getDefendPosition(), PlayerState::Self, [&](auto &u) {
                return u->getRole() == Role::Worker && !u->getGoal().isValid() && (!u->hasResource() || !u->getResource().lock()->getType().isRefinery()) && !unit.getBuildPosition().isValid();
            });

            auto combatCount = Roles::getMyRoleCount(Role::Combat) - (unit.getRole() == Role::Combat ? 1 : 0);
            auto combatWorkersCount =  Roles::getMyRoleCount(Role::Combat) - com(Protoss_Zealot) - com(Protoss_Dragoon) - com(Zerg_Zergling) - (unit.getRole() == Role::Combat ? 1 : 0);

            const auto healthyWorker = [&] {

                // Don't pull low shield probes
                if (unit.getType() == Protoss_Probe && unit.getShields() <= 4)
                    return false;

                // Don't pull low health drones
                if (unit.getType() == Zerg_Drone && unit.getHealth() < 20)
                    return false;
                return true;
            };

            // Proactive pulls will result in the worker defending
            const auto proactivePullWorker = [&]() {

                // If this isn't the closest mineral worker to the defend position, don't pull it
                if (unit.getRole() == Role::Worker && closestWorker && unit != *closestWorker)
                    return false;

                // Protoss
                if (Broodwar->self()->getRace() == Races::Protoss) {
                    int completedDefenders = com(Protoss_Photon_Cannon) + com(Protoss_Zealot);
                    int visibleDefenders = vis(Protoss_Photon_Cannon) + vis(Protoss_Zealot);

                    // If trying to hide tech, pull 1 probe with a Zealot
                    if (!BuildOrder::isRush() && BuildOrder::isHideTech() && combatCount < 2 && completedDefenders > 0)
                        return true;

                    // If trying to FFE, pull based on Cannon/Zealot numbers, or lack of scouting information
                    if (BuildOrder::getCurrentBuild() == "FFE") {
                        if (Spy::enemyRush() && Spy::getEnemyOpener() == "4Pool" && combatCount < 8 - (2 * completedDefenders) && visibleDefenders >= 1)
                            return true;
                        if (Spy::enemyPressure() && combatCount < 8 - (2 * completedDefenders) && visibleDefenders >= 2)
                            return true;
                        if (Spy::enemyRush() && Spy::getEnemyOpener() == "9Pool" && Util::getTime() > Time(3, 15) && completedDefenders < 3)
                            return combatWorkersCount < 3;
                        if (!Terrain::getEnemyStartingPosition().isValid() && Spy::getEnemyBuild() == "Unknown" && combatCount < 2 && completedDefenders < 1 && visibleDefenders > 0)
                            return true;
                    }

                    // If trying to 2Gate at our natural, pull based on Zealot numbers
                    else if (BuildOrder::getCurrentBuild() == "2Gate" && BuildOrder::getCurrentOpener() == "Natural") {
                        if (Spy::enemyRush() && combatCount < 8 - (2 * completedDefenders) && visibleDefenders >= 1)
                            return true;
                        if (Spy::enemyPressure() && combatCount < 8 - (2 * completedDefenders) && visibleDefenders >= 2)
                            return true;
                    }

                    // If trying to 1GateCore and scouted 2Gate late, pull workers to block choke when we are ready
                    else if (BuildOrder::getCurrentBuild() == "1GateCore" && Spy::getEnemyBuild() == "2Gate" && BuildOrder::getCurrentTransition() != "Defensive" && holdChoke) {
                        if (Util::getTime() < Time(3, 30) && combatWorkersCount < 2)
                            return true;
                    }
                }

                // Terran

                // Zerg
                if (Broodwar->self()->getRace() == Races::Zerg) {
                    if (BuildOrder::getCurrentOpener() == "12Pool" && Spy::getEnemyOpener() == "9Pool" && total(Zerg_Zergling) < 16 && int(Stations::getStations(PlayerState::Self).size()) >= 2)
                        return combatWorkersCount < 3;
                }
                return false;
            };

            // Reactive pulls will cause the worker to attack aggresively
            const auto reactivePullWorker = [&]() {

                auto proxyDangerousBuilding = Util::getClosestUnit(BWEB::Map::getMainPosition(), PlayerState::Enemy, [&](auto &u) {
                    return u->isProxy() && u->getType().isBuilding() && u->canAttackGround();
                });
                auto proxyBuildingWorker = Util::getClosestUnit(BWEB::Map::getMainPosition(), PlayerState::Enemy, [&](auto &u) {
                    return u->getType().isWorker() && (u->isThreatening() || (proxyDangerousBuilding && u->getType().isWorker() && u->getPosition().getDistance(proxyDangerousBuilding->getPosition()) < 160.0));
                });

                // HACK: Don't pull workers reactively versus vultures
                if (Players::getVisibleCount(PlayerState::Enemy, Terran_Vulture) > 0)
                    return false;
                if (Spy::getEnemyBuild() == "2Gate" && Spy::enemyProxy())
                    return false;

                // If we have immediate threats
                if (Players::ZvT() && proxyDangerousBuilding && com(Zerg_Zergling) <= 2)
                    return combatWorkersCount < 6;
                if (Players::ZvP() && proxyDangerousBuilding && Spy::getEnemyBuild() == "CannonRush" && com(Zerg_Zergling) <= 2)
                    return combatWorkersCount < (4 * Players::getVisibleCount(PlayerState::Enemy, proxyDangerousBuilding->getType()));
                if (Spy::getWorkersNearUs() > 2 && com(Zerg_Zergling) < Spy::getWorkersNearUs())
                    return Spy::getWorkersNearUs() >= combatWorkersCount - 3;
                if (BuildOrder::getCurrentOpener() == "12Hatch" && Spy::getEnemyOpener() == "8Rax" && com(Zerg_Zergling) < 2)
                    return combatWorkersCount <= com(Zerg_Drone) - 4;

                // If we're trying to make our expanding hatchery and the drone is being harassed
                if (Players::ZvP() && vis(Zerg_Hatchery) == 1 && Util::getTime() < Time(3, 00) && BuildOrder::isOpener() && Units::getImmThreat() > 0.0f && combatCount == 0)
                    return combatWorkersCount < 1;
                if (Players::ZvP() && Util::getTime() < Time(4, 00) && int(Stations::getStations(PlayerState::Self).size()) < 2 && BuildOrder::getBuildQueue()[Zerg_Hatchery] >= 2 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Probe) > 0)
                    return combatWorkersCount < 1;

                if (Players::ZvZ() && Spy::getEnemyOpener() == "4Pool" && Units::getImmThreat() > 0.0f && com(Zerg_Zergling) == 0)
                    return combatWorkersCount < 10;

                // If we suspect a cannon rush is coming
                if (Players::ZvP() && Spy::enemyPossibleProxy() && Util::getTime() < Time(3, 00))
                    return combatWorkersCount < 1;
                return false;
            };

            // Check if workers should fight or work
            if (unit.getType().isWorker()) {
                auto react = reactivePullWorker();
                auto proact = proactivePullWorker();

                // Pull a worker if needed
                if (unit.getRole() == Role::Worker && !unit.unit()->isCarryingMinerals() && !unit.unit()->isCarryingGas() && healthyWorker() && (react || proact)) {
                    unit.setRole(Role::Combat);
                    unit.setBuildingType(None);
                    unit.setBuildPosition(TilePositions::Invalid);
                    lastRoleChange = Broodwar->getFrameCount();
                }

                // Return a worker if not needed
                else if (unit.getRole() == Role::Combat && ((!react && !proact) || !healthyWorker())) {
                    unit.setRole(Role::Worker);
                    lastRoleChange = Broodwar->getFrameCount();
                }

                // HACK: Check if this was a reactive pull, set worker to always engage
                if (unit.getRole() == Role::Combat) {
                    combatCount--;
                    combatWorkersCount--;
                    react = reactivePullWorker();
                    if (react && !unit.hasTarget()) {
                        unit.setLocalState(LocalState::Attack);
                        unit.setGlobalState(GlobalState::Attack);
                    }
                }
            }
        }

        void updateCombatUnits()
        {
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                auto &unit = *u;

                // TODO: Move to Roles
                if (unit.getType().isWorker())
                    updateRole(unit);

                if (unit.getRole() == Role::Combat) {
                    Simulation::update(unit);
                    State::update(unit);
                    Destination::update(unit);
                }
            }

            Clusters::onFrame();
            Formations::onFrame();

            for (auto &cluster : Clusters::getClusters()) {

                // Update the commander first
                auto commander = cluster.commander.lock();
                if (commander) {
                    Navigation::update(*commander);
                    Decision::update(*commander);
                }

                // Update remaining units
                for (auto &unit : cluster.units) {
                    auto sharedDecision = cluster.commandShare == CommandShare::Exact && !unit->localRetreat() && !unit->globalRetreat() && !unit->isNearSuicide()
                        && !unit->attemptingRegroup() && (unit->getType() == commander->getType() || unit->getLocalState() != LocalState::Attack);

                    if (unit->attemptingRegroup())
                        unit->circle(Colors::Orange);

                    if (sharedDecision) {
                        if (commander->getCommandType() == UnitCommandTypes::Attack_Unit)
                            unit->command(commander->getCommandType(), *commander->getTarget().lock());
                        else if (commander->getCommandType() == UnitCommandTypes::Move && !unit->isTargetedBySplash())
                            unit->command(commander->getCommandType(), commander->getCommandPosition());
                        else if (commander->getCommandType() == UnitCommandTypes::Right_Click_Position && !unit->isTargetedBySplash())
                            unit->command(UnitCommandTypes::Right_Click_Position, commander->getCommandPosition());
                        else {
                            Navigation::update(*unit);
                            Decision::update(*unit);
                        }
                    }
                    else {
                        Navigation::update(*unit);
                        Decision::update(*unit);
                    }
                }
            }
        }
    }

    void onFrame() {
        Visuals::startPerfTest();
        updateCombatUnits();
        checkHoldChoke();
        Visuals::endPerfTest("Combat");
    }

    void onStart() {

    }

    bool defendChoke() { return holdChoke; }
}