#include "..\McRave\McRave.h"

using namespace BWAPI;
using namespace std;

namespace McRave::Horizon {

    namespace {
        bool ignoreSim = false;
        double minThreshold = 0.0;
        double maxThreshold = 0.0;

        double prepTime(UnitInfo& unit) {
            if (unit.getType() == UnitTypes::Terran_Siege_Tank_Tank_Mode)
                return 65.0 / 24.0;
            if (unit.getType() == UnitTypes::Zerg_Lurker && !unit.isBurrowed())
                return 36.0 / 24.0;
            return 0.0;
        };
    }

    void updateThresholds(UnitInfo& unit)
    {
        double minWinPercent = 0.6;
        double maxWinPercent = 1.2;
        auto baseCountSwing = Util::getTime() > Time(5, 00) ? max(0.0, (double(Stations::getMyStations().size()) - double(Stations::getEnemyStations().size())) / 20) : 0.0;
        auto baseDistSwing = Util::getTime() > Time(5, 00) ? unit.getEngagePosition().getDistance(Terrain::getEnemyStartingPosition()) / (10 * BWEB::Map::getMainPosition().getDistance(Terrain::getEnemyStartingPosition())) : 0.0;

        // P
        if (Players::PvP()) {
            minWinPercent = 0.80;
            maxWinPercent = 1.20;
        }
        if (Players::PvZ()) {
            minWinPercent = 0.6;
            maxWinPercent = 1.2;
        }
        if (Players::PvT()) {
            minWinPercent = 0.6 - baseCountSwing;
            maxWinPercent = 1.0 - baseCountSwing;
        }

        // Z
        if (Players::ZvP()) {
            minWinPercent = 0.90;
            maxWinPercent = 1.30;
        }
        if (Players::ZvZ()) {
            minWinPercent = 1.00;
            maxWinPercent = 1.20;
        }
        if (Players::ZvT()) {
            minWinPercent = 0.8 - baseCountSwing + baseDistSwing;
            maxWinPercent = 1.2 - baseCountSwing + baseDistSwing;
        }

        minThreshold = minWinPercent;
        maxThreshold = maxWinPercent;
    }

    void simulate(UnitInfo& unit)
    {
        updateThresholds(unit);
        auto enemyLocalGroundStrength = 0.0, allyLocalGroundStrength = 0.0;
        auto enemyLocalAirStrength = 0.0, allyLocalAirStrength = 0.0;
        auto unitToEngage = unit.getSpeed() > 0.0 ? unit.getEngDist() / (24.0 * unit.getSpeed()) : 5.0;
        auto simulationTime = unitToEngage + max(5.0, Players::getSupply(PlayerState::Self, Races::None) / 20.0) + prepTime(unit);
        auto belowGrdtoGrdLimit = false;
        auto belowGrdtoAirLimit = false;
        auto belowAirtoAirLimit = false;
        auto belowAirtoGrdLimit = false;

        // If we have excessive resources, ignore our simulation and engage
        if (!ignoreSim && Broodwar->self()->minerals() >= 2000 && Broodwar->self()->gas() >= 2000 && Players::getSupply(PlayerState::Self, Races::None) >= 380)
            ignoreSim = true;
        if (ignoreSim && (Broodwar->self()->minerals() <= 500 || Broodwar->self()->gas() <= 500 || Players::getSupply(PlayerState::Self, Races::None) <= 240))
            ignoreSim = false;

        if (ignoreSim) {
            unit.setSimState(SimState::Win);
            unit.setSimValue(10.0);
            return;
        }
        else if (unit.getEngDist() == DBL_MAX) {
            unit.setSimState(SimState::Loss);
            unit.setSimValue(0.0);
            return;
        }

        const auto addToSim = [&](UnitInfo& u) {
            if (!u.unit()
                || (u.getType().isWorker() && ((u.unit()->exists() && u.unit()->getOrder() != Orders::AttackUnit) || !u.hasAttackedRecently()))
                || (!u.unit()->isCompleted() && u.unit()->exists())
                || (u.unit()->exists() && (u.unit()->isStasised() || u.unit()->isMorphing()))
                || (u.getVisibleAirStrength() <= 0.0 && u.getVisibleGroundStrength() <= 0.0)
                || (!u.hasTarget())
                || (u.getRole() != Role::None && u.getRole() != Role::Combat && u.getRole() != Role::Defender))
                return false;
            return true;
        };

        const auto addBonus = [&](UnitInfo& u, double &simRatio) {
            if (u.isHidden())
                simRatio *= 2.0;
            if (!u.getType().isFlyer() && u.getGroundRange() > 32.0 && Broodwar->getGroundHeight(u.getTilePosition()) > Broodwar->getGroundHeight(TilePosition(u.getTarget().getEngagePosition())))
                simRatio *= 2.0;
            return;
        };

        for (auto &e : Units::getUnits(PlayerState::Enemy)) {
            UnitInfo &enemy = *e;
            if (!addToSim(enemy))
                continue;

            auto simRatio =                     0.0;
            auto distTarget =                   double(Util::boxDistance(enemy.getType(), enemy.getPosition(), unit.getType(), unit.getPosition()));
            auto distEngage =                   double(Util::boxDistance(enemy.getType(), enemy.getPosition(), unit.getType(), unit.getEngagePosition()));
            const auto range =                  enemy.getTarget().getType().isFlyer() ? enemy.getAirRange() : enemy.getGroundRange();
            const auto enemyReach =             max(enemy.getAirReach(), enemy.getGroundReach());

            // If the unit doesn't affect this simulation
            if ((enemy.getSpeed() <= 0.0 && distEngage > range + 32.0 && distTarget > range + 32.0)
                || (enemy.getType() == UnitTypes::Terran_Siege_Tank_Siege_Mode && distTarget < 64.0)
                || (enemy.getSpeed() <= 0.0 && distTarget > 0.0 && enemy.getTarget().getSpeed() <= 0.0)
                || (enemy.targetsFriendly() && unit.hasTarget() && enemy.getPosition().getDistance(unit.getTarget().getPosition()) >= enemyReach)
                /*|| (!enemy.targetsFriendly() && enemy.getEngagePosition().getDistance(unit.getEngagePosition()) > enemyReach && enemy.getPosition().getDistance(unit.getPosition()) > enemyReach)*/)
                continue;

            // If enemy doesn't move, calculate how long it will remain in range once in range
            if (enemy.getSpeed() <= 0.0) {
                const auto distance =               distTarget;
                const auto speed =                  enemy.getTarget().getSpeed() * 24.0;
                const auto engageTime =             max(0.0, (distance - range) / speed);
                simRatio =                          max(0.0, simulationTime - engageTime);
            }

            // If enemy can move, calculate how quickly it can engage
            else {
                const auto distance =               min(distTarget, distEngage);
                const auto speed =                  enemy.getSpeed() * 24.0;
                const auto engageTime =             max(0.0, (distance - range) / speed);
                simRatio =                          max(0.0, simulationTime - engageTime);
            }

            // Add their values to the simulation
            addBonus(enemy, simRatio);
            if (enemy.canAttackAir() && enemy.canAttackGround()) {
                enemyLocalGroundStrength += max(enemy.getVisibleGroundStrength(), enemy.getVisibleAirStrength()) * simRatio;
                enemyLocalAirStrength += max(enemy.getVisibleGroundStrength(), enemy.getVisibleAirStrength()) * simRatio;
            }
            else {
                enemyLocalGroundStrength += enemy.getVisibleGroundStrength() * simRatio;
                enemyLocalAirStrength += enemy.getVisibleAirStrength() * simRatio;
            }
        }

        for (auto &a : Units::getUnits(PlayerState::Self)) {
            UnitInfo &ally = *a;
            if (!addToSim(ally))
                continue;

            const auto allyRange = max(ally.getAirRange(), ally.getGroundRange());
            const auto allyReach = max(ally.getAirReach(), ally.getGroundReach());
            const auto distance = ally.getEngDist();
            const auto speed = ally.getSpeed() > 0.0 ? ally.getSpeed() * 24.0 : unit.getSpeed() * 24.0;
            auto simRatio = simulationTime - (distance / speed) - prepTime(ally);

            // If the unit doesn't affect this simulation
            if (ally.localRetreat()
                || (ally.getSpeed() <= 0.0 && ally.getEngDist() > -16.0)
                || (unit.hasTarget() && ally.hasTarget() && ally.getEngagePosition().getDistance(unit.getTarget().getPosition()) > allyReach))
                continue;

            // Add their values to the simulation
            addBonus(ally, simRatio);
            allyLocalGroundStrength += ally.getVisibleGroundStrength() * simRatio;
            allyLocalAirStrength += ally.getVisibleAirStrength() * simRatio;

            // Check if any ally are below the limit to synhcronize sim values
            if (ally.hasTarget() && unit.hasTarget() && ally.getTarget() == unit.getTarget() && ally.getSimValue() <= minThreshold && ally.getSimValue() != 0.0) {
                ally.isFlying() ?
                    (ally.getTarget().isFlying() ? belowAirtoAirLimit = true : belowAirtoGrdLimit = true) :
                    (ally.getTarget().isFlying() ? belowGrdtoAirLimit = true : belowGrdtoGrdLimit = true);
            }
        }

        // Assign the sim value
        const auto attackAirAsAir =         enemyLocalAirStrength > 0.0 ? allyLocalAirStrength / enemyLocalAirStrength : 10.0;
        const auto attackAirAsGround =      enemyLocalGroundStrength > 0.0 ? allyLocalAirStrength / enemyLocalGroundStrength : 10.0;
        const auto attackGroundAsAir =      enemyLocalAirStrength > 0.0 ? allyLocalGroundStrength / enemyLocalAirStrength : 10.0;
        const auto attackGroundAsGround =   enemyLocalGroundStrength > 0.0 ? allyLocalGroundStrength / enemyLocalGroundStrength : 10.0;

        if (!unit.hasTarget())
            unit.getType().isFlyer() ? unit.setSimValue(min(attackAirAsAir, attackGroundAsAir)) : unit.setSimValue(min(attackAirAsGround, attackGroundAsGround));
        else
            unit.getType().isFlyer() ? unit.setSimValue(unit.getTarget().getType().isFlyer() ? attackAirAsAir : attackGroundAsAir) : unit.setSimValue(unit.getTarget().getType().isFlyer() ? attackAirAsGround : attackGroundAsGround);

        auto engagedAlreadyOffset = unit.getSimState() == SimState::Win ? 0.2 : 0.0;

        // If above/below thresholds, it's a sim win/loss
        if (unit.getSimValue() >= maxThreshold - engagedAlreadyOffset)
            unit.setSimState(SimState::Win);
        else if (unit.getSimValue() < minThreshold - engagedAlreadyOffset || (unit.getSimState() == SimState::None && unit.getSimValue() < maxThreshold))
            unit.setSimState(SimState::Loss);

        // Check for hardcoded directional losses
        if (unit.hasTarget() && unit.getSimValue() < maxThreshold - engagedAlreadyOffset) {
            if (unit.isFlying()) {
                if (unit.getTarget().isFlying() && belowAirtoAirLimit)
                    unit.setSimState(SimState::Loss);
                else if (unit.getTarget().isFlying() && belowAirtoGrdLimit)
                    unit.setSimState(SimState::Loss);
            }
            else {
                if (unit.getTarget().isFlying() && belowGrdtoAirLimit)
                    unit.setSimState(SimState::Loss);
                else if (!unit.getTarget().isFlying() && belowGrdtoGrdLimit)
                    unit.setSimState(SimState::Loss);
            }
        }
    }
}