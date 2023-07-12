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
        double immThreat;
        Position enemyArmyCenter;

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
                    if (unit.isThreatening() && !unit.getType().isWorker()) {
                        immThreat += unit.getVisibleGroundStrength();
                        unit.circle(Colors::Red);
                    }

                    // Add to army center
                    if (unit.getPosition().isValid() && !unit.getType().isBuilding() && !unit.getType().isWorker() && !unit.movedFlag) {
                        enemyArmyCenter += unit.getPosition();
                        enemyArmyCount++;
                    }

                    //Broodwar->drawTextMap(unit.getPosition(), "%.2f", unit.getPriority());
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

            for (auto &p : Players::getPlayers()) {
                PlayerInfo &player = p.second;
                if (player.isSelf()) {
                    for (auto &u : player.getUnits()) {
                        UnitInfo &unit = *u;

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
}