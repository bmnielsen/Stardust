#include "McRave.h"
#include "Events.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Units {

    namespace {

        set<shared_ptr<UnitInfo>> enemyUnits;
        set<shared_ptr<UnitInfo>> myUnits;
        set<shared_ptr<UnitInfo>> neutralUnits;
        set<shared_ptr<UnitInfo>> allyUnits;
        map<UnitSizeType, int> allyGrdSizes;
        map<UnitSizeType, int> enemyGrdSizes;
        map<UnitSizeType, int> allyAirSizes;
        map<UnitSizeType, int> enemyAirSizes;
        map<Role, int> myRoles;
        double immThreat;
        Position enemyArmyCenter;

        void updateRole(UnitInfo& unit)
        {
            // Don't assign a role to uncompleted units
            if (!unit.unit()->isCompleted() && !unit.getType().isBuilding() && unit.getType() != Zerg_Egg) {
                unit.setRole(Role::None);
                return;
            }

            // Update default role
            if (unit.getRole() == Role::None) {
                if (unit.getType().isWorker())
                    unit.setRole(Role::Worker);
                else if ((unit.getType().isBuilding() && !unit.canAttackGround() && !unit.canAttackAir() && unit.getType() != Zerg_Creep_Colony) || unit.getType() == Zerg_Larva || unit.getType() == Zerg_Egg)
                    unit.setRole(Role::Production);
                else if (unit.getType().isBuilding() && (unit.canAttackGround() || unit.canAttackAir() || unit.getType() == Zerg_Creep_Colony))
                    unit.setRole(Role::Defender);
                else if ((unit.getType().isDetector() && !unit.getType().isBuilding()) || unit.getType() == Protoss_Arbiter)
                    unit.setRole(Role::Support);
                else if (unit.getType().spaceProvided() > 0)
                    unit.setRole(Role::Transport);
                else
                    unit.setRole(Role::Combat);
            } 

            // Check if a worker morphed into a building
            if (unit.getRole() == Role::Worker && unit.getType().isBuilding()) {
                if (unit.getType().isBuilding() && !unit.canAttackGround() && !unit.canAttackAir() && unit.getType() != Zerg_Creep_Colony)
                    unit.setRole(Role::Production);
                else
                    unit.setRole(Role::Defender);
            }
        }

        void updateEnemies()
        {
            enemyArmyCenter = Position(0, 0);
            auto enemyArmyCount = 0;

            for (auto &p : Players::getPlayers()) {
                PlayerInfo &player = p.second;
                if (!player.isEnemy())
                    continue;

                // Iterate and update important information
                for (auto &u : player.getUnits()) {
                    UnitInfo &unit = *u;

                    // If this is a flying building that we haven't recognized as being a flyer, remove overlap tiles
                    auto flyingBuilding = unit.unit()->exists() && !unit.isFlying() && (unit.unit()->getOrder() == Orders::LiftingOff || unit.unit()->getOrder() == Orders::BuildingLiftOff || unit.unit()->isFlying());
                    if (flyingBuilding && unit.getLastTile().isValid())
                        Events::onUnitLift(unit);

                    // Update
                    unit.update();

                    if (unit.getType().isBuilding() && !unit.isFlying() && unit.getTilePosition().isValid() && BWEB::Map::isUsed(unit.getTilePosition()) == None && Broodwar->isVisible(TilePosition(unit.getPosition())))
                        Events::onUnitLand(unit);

                    // Must see a 3x3 grid of Tiles to set a unit to invalid position
                    if (!unit.unit()->exists() && Broodwar->isVisible(TilePosition(unit.getPosition())) && (!unit.isBurrowed() || Actions::overlapsDetection(unit.unit(), unit.getPosition(), PlayerState::Self) || (unit.getWalkPosition().isValid() && Grids::getAGroundCluster(unit.getWalkPosition()) > 0)))
                        Events::onUnitDisappear(unit);

                    // If a unit is threatening our position
                    if (unit.isThreatening())
                        immThreat += unit.getVisibleGroundStrength();                    

                    // Add to army center
                    if (unit.getPosition().isValid() && !unit.getType().isBuilding() && !unit.getType().isWorker() && !unit.movedFlag) {
                        enemyArmyCenter += unit.getPosition();
                        enemyArmyCount++;
                    }
                }
            }

            enemyArmyCenter /= enemyArmyCount;
        }

        void updateAllies()
        {

        }

        void updateSelf()
        {
            for (auto &p : Players::getPlayers()) {
                PlayerInfo &player = p.second;
                if (!player.isSelf())
                    continue;

                // Iterate and update important information
                for (auto &u : player.getUnits()) {
                    UnitInfo &unit = *u;

                    unit.update();
                    updateRole(unit);
                }
            }
        }

        void updateNeutrals()
        {
            // Neutrals
            for (auto &p : Players::getPlayers()) {
                PlayerInfo &player = p.second;
                if (!player.isNeutral())
                    continue;

                for (auto &u : player.getUnits()) {
                    UnitInfo &unit = *u;
                    neutralUnits.insert(u);

                    unit.update();

                    if (!unit.unit() || !unit.unit()->exists())
                        continue;
                }
            }
        }

        void updateCounters()
        {
            immThreat = 0.0;

            enemyUnits.clear();
            myUnits.clear();
            neutralUnits.clear();
            allyUnits.clear();

            allyGrdSizes.clear();
            enemyGrdSizes.clear();

            myRoles.clear();

            for (auto &p : Players::getPlayers()) {
                PlayerInfo &player = p.second;
                if (player.isSelf()) {
                    for (auto &u : player.getUnits()) {
                        UnitInfo &unit = *u;

                        myRoles[unit.getRole()]++;
                        myUnits.insert(u);
                        if (unit.getRole() == Role::Combat)
                            unit.getType().isFlyer() ? allyAirSizes[unit.getType().size()]++ : allyGrdSizes[unit.getType().size()]++;
                    }
                }
                if (player.isEnemy()) {
                    for (auto &u : player.getUnits()) {
                        UnitInfo &unit = *u;

                        enemyUnits.insert(u);
                        if (!unit.getType().isBuilding() && !unit.getType().isWorker())
                            unit.getType().isFlyer() ? enemyAirSizes[unit.getType().size()]++ : enemyGrdSizes[unit.getType().size()]++;
                    }
                }
            }
        }

        void updateUnits()
        {
            updateEnemies();
            updateAllies();
            updateNeutrals();
            updateSelf();
        }
    }

    void onFrame()
    {
        Visuals::startPerfTest();
        updateCounters();
        updateUnits();
        Visuals::endPerfTest("Units");
    }

    shared_ptr<UnitInfo> getUnitInfo(Unit unit)
    {
        for (auto &[_, player] : Players::getPlayers()) {
            for (auto &u : player.getUnits()) {
                if (u->unit() == unit)
                    return u;
            }
        }
        return nullptr;
    }

    set<shared_ptr<UnitInfo>>& getUnits(PlayerState state)
    {
        switch (state) {

        case PlayerState::Ally:
            return allyUnits;
        case PlayerState::Enemy:
            return enemyUnits;
        case PlayerState::Neutral:
            return neutralUnits;
        case PlayerState::Self:
            return myUnits;
        }
        return myUnits;
    }

    map<UnitSizeType, int>& getAllyGrdSizes() { return allyGrdSizes; }
    map<UnitSizeType, int>& getEnemyGrdSizes() { return enemyGrdSizes; }
    map<UnitSizeType, int>& getAllyAirSizes() { return allyAirSizes; }
    map<UnitSizeType, int>& getEnemyAirSizes() { return enemyAirSizes; }
    Position getEnemyArmyCenter() { return enemyArmyCenter; }
    double getImmThreat() { return immThreat; }
    int getMyRoleCount(Role role) { return myRoles[role]; }
}