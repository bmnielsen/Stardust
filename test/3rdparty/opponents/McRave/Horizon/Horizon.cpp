#include "..\McRave\McRave.h"

using namespace BWAPI;
using namespace std;

namespace McRave::Horizon {

    namespace {

        struct SimStrength {
            double air = 0.0;
            double ground = 0.0;
        };

        bool addToSim(UnitInfo& u) {
            if (!u.unit()
                || (u.getType().isWorker() && Util::getTime() > Time(6, 00) && ((u.unit()->exists() && u.unit()->getOrder() != Orders::AttackUnit) || !u.hasAttackedRecently()))
                || (u.unit()->exists() && !u.unit()->isCompleted())
                || (u.unit()->exists() && (u.unit()->isStasised() || u.unit()->isMorphing()))
                || (u.getVisibleAirStrength() <= 0.0 && u.getVisibleGroundStrength() <= 0.0)
                || (u.getRole() != Role::None && u.getRole() != Role::Combat && u.getRole() != Role::Defender)
                || !u.hasTarget())
                return false;
            return true;
        }

        double perpDist(Position p0, Position p1, Position p2) {
            return abs(double((p2.x - p1.x) * (p1.y - p0.y) - (p1.x - p0.x) * (p2.y - p1.y))) / p1.getDistance(p2);
        }

        void addBonus(UnitInfo& u, UnitInfo& t, double &simRatio) {
            if (u.isHidden())
                simRatio *= 2.0;
            if (!u.isFlying() && !t.isFlying() && u.getGroundRange() > 32.0 && Broodwar->getGroundHeight(u.getTilePosition()) > Broodwar->getGroundHeight(TilePosition(t.getEngagePosition())))
                simRatio *= 2.0;
            if (u.getType().isWorker() && Util::getTime() < Time(6, 00))
                simRatio /= 1.5;
            return;
        }

        double addPrepTime(UnitInfo& unit) {
            if (unit.getType() == UnitTypes::Terran_Siege_Tank_Tank_Mode)
                return 65.0 / 24.0;
            if (unit.getType() == UnitTypes::Zerg_Lurker && !unit.isBurrowed())
                return 36.0 / 24.0;
            return 0.0;
        }
    }

    void simulate(UnitInfo& unit)
    {
        if (!unit.hasTarget())
            return;

        auto &unitTarget = unit.getTarget().lock();
        const auto unitToEngage = unit.getSpeed() > 0.0 ? unit.getEngDist() / (24.0 * unit.getSpeed()) : 5.0;
        const auto simulationTime = unitToEngage + 5.0 + addPrepTime(unit);
        const auto targetDisplacement = unitToEngage * unitTarget->getSpeed() * 24.0;
        map<Player, SimStrength> simStrengthPerPlayer;

        for (auto &e : Units::getUnits(PlayerState::Enemy)) {
            UnitInfo &enemy = *e;
            if (!addToSim(enemy))
                continue;

            auto &enemyTarget =                 enemy.getTarget().lock();
            auto simRatio =                     0.0;
            const auto distTarget =             max(0.0, double(Util::boxDistance(enemy.getType(), enemy.getPosition(), unit.getType(), unit.getPosition())) - targetDisplacement);
            const auto distEngage =             max(0.0, double(Util::boxDistance(enemy.getType(), enemy.getPosition(), unit.getType(), unit.getEngagePosition())) - targetDisplacement);
            //const auto distPerp =               perpDist(enemy.getPosition(), unit.getPosition(), unit.getEngagePosition());
            const auto range =                  enemyTarget->getType().isFlyer() ? enemy.getAirRange() : enemy.getGroundRange();
            const auto enemyReach =             max(enemy.getAirReach(), enemy.getGroundReach());

            // If the unit doesn't affect this simulation
            if ((enemy.getSpeed() <= 0.0 && distEngage > range + 32.0 && distTarget > range + 32.0 /*&& distPerp > range*/)
                || (enemy.getType() == UnitTypes::Terran_Siege_Tank_Siege_Mode && distTarget < 64.0)
                || (enemy.getSpeed() <= 0.0 && distTarget > range /*&& distPerp > range*/ && enemyTarget->getSpeed() <= 0.0)
                || (enemy.targetsFriendly() && unit.hasTarget() && enemy.getPosition().getDistance(unitTarget->getPosition()) >= enemyReach))
                continue;

            // If enemy doesn't move, calculate how long it will remain in range once in range
            if (enemy.getSpeed() <= 0.0) {
                const auto distance =               distTarget;// min(distPerp, distTarget);
                const auto speed =                  enemyTarget->getSpeed() * 24.0;
                const auto engageTime =             max(0.0, (distance - range) / speed);
                simRatio =                          max(0.0, simulationTime - engageTime);
            }

            // If enemy can move, calculate how quickly it can engage
            else {
                const auto distance =               min(distTarget, distEngage); //min({ distPerp, distTarget, distEngage });
                const auto speed =                  enemy.getSpeed() * 24.0;
                const auto engageTime =             max(0.0, (distance - range) / speed);
                simRatio =                          max(0.0, simulationTime - engageTime);
            }

            // Add their values to the simulation
            addBonus(enemy, *enemyTarget, simRatio);
            if (enemy.canAttackAir() && enemy.canAttackGround()) {
                simStrengthPerPlayer[enemy.getPlayer()].ground += max(enemy.getVisibleGroundStrength(), enemy.getVisibleAirStrength()) * simRatio;
                simStrengthPerPlayer[enemy.getPlayer()].air += max(enemy.getVisibleGroundStrength(), enemy.getVisibleAirStrength()) * simRatio;
            }
            else {
                simStrengthPerPlayer[enemy.getPlayer()].ground += enemy.getVisibleGroundStrength() * simRatio;
                simStrengthPerPlayer[enemy.getPlayer()].air += enemy.getVisibleAirStrength() * simRatio;
            }
        }

        for (auto &a : Units::getUnits(PlayerState::Self)) {
            UnitInfo &self = *a;
            if (!addToSim(self))
                continue;

            auto &selfTarget = self.getTarget().lock();
            const auto range = max(self.getAirRange(), self.getGroundRange());
            const auto reach = max(self.getAirReach(), self.getGroundReach());
            const auto distance = double(Util::boxDistance(self.getType(), self.getPosition(), unitTarget->getType(), unitTarget->getPosition()));
            const auto speed = self.getSpeed() > 0.0 ? self.getSpeed() * 24.0 : unit.getSpeed() * 24.0;
            const auto engageTime = max(0.0, (distance - range) / speed);
            auto simRatio = max(0.0, simulationTime - engageTime + addPrepTime(self));

            // If the unit doesn't affect this simulation
            if (self.localRetreat()
                || self.globalRetreat()
                || (self.getSpeed() <= 0.0 && self.getEngDist() > -16.0)
                || (unit.hasTarget() && self.hasTarget() && self.getEngagePosition().getDistance(unitTarget->getPosition()) > reach))
                continue;

            // Add their values to the simulation
            addBonus(self, *selfTarget, simRatio);
            simStrengthPerPlayer[self.getPlayer()].ground += self.getVisibleGroundStrength() * simRatio;
            simStrengthPerPlayer[self.getPlayer()].air += self.getVisibleAirStrength() * simRatio;
        }

        for (auto &a : Units::getUnits(PlayerState::Ally)) {
            UnitInfo &ally = *a;
            if (!addToSim(ally))
                continue;

            auto &allyTarget = ally.getTarget().lock();
            const auto range = max(ally.getAirRange(), ally.getGroundRange());
            const auto reach = max(ally.getAirReach(), ally.getGroundReach());
            const auto distance = double(Util::boxDistance(ally.getType(), ally.getPosition(), unit.getType(), unitTarget->getPosition()));
            const auto speed = ally.getSpeed() > 0.0 ? ally.getSpeed() * 24.0 : unit.getSpeed() * 24.0;
            const auto engageTime = max(0.0, (distance - range) / speed);
            auto simRatio = max(0.0, simulationTime - engageTime + addPrepTime(ally));

            // If the unit doesn't affect this simulation
            if ((ally.getSpeed() <= 0.0 && ally.getEngDist() > -16.0)
                || (unit.hasTarget() && ally.hasTarget() && ally.getEngagePosition().getDistance(unitTarget->getPosition()) > reach))
                continue;

            // Add their values to the simulation
            addBonus(ally, *allyTarget, simRatio);
            simStrengthPerPlayer[ally.getPlayer()].ground += ally.getVisibleGroundStrength() * simRatio;
            simStrengthPerPlayer[ally.getPlayer()].air += ally.getVisibleAirStrength() * simRatio;
        }

        // Determine sim value based on max of enemy forces and max of self/ally forces
        auto addForces = Broodwar->getGameType() == GameTypes::Top_vs_Bottom;
        auto enemyAirStrength = 0.0, enemyGroundStrength = 0.0;
        auto allyAirStrength = 0.0, allyGroundStrength = 0.0;
        for (auto &[player, sim] : simStrengthPerPlayer) {
            auto enemy = player->isEnemy(Broodwar->self());
            if (enemy) {
                addForces ? enemyGroundStrength += sim.ground : enemyGroundStrength = max(enemyGroundStrength, sim.ground);
                addForces ? enemyAirStrength += sim.air : enemyAirStrength = max(enemyAirStrength, sim.air);
            }
            else {
                addForces ? allyGroundStrength = sim.ground : allyGroundStrength = max(allyGroundStrength, sim.ground);
                addForces ? allyAirStrength = sim.air : allyAirStrength = max(allyAirStrength, sim.air);
            }
        }

        // Assign the sim value
        const auto attackAirAsAir =         enemyAirStrength > 0.0 ? allyAirStrength / enemyAirStrength : 10.0;
        const auto attackAirAsGround =      enemyGroundStrength > 0.0 ? allyAirStrength / enemyGroundStrength : 10.0;
        const auto attackGroundAsGround =   enemyGroundStrength > 0.0 ? allyGroundStrength / enemyGroundStrength : 10.0;


        auto attackGroundAsAir =      enemyAirStrength > 0.0 ? allyGroundStrength / enemyAirStrength : 10.0;
        // HACK: We don't have a way right now with Horizon to look at "going solo" when sync assumed we win
        // for example if my air + ground > ground, but our ground vs ground wont fight, we need to assume worst case (going solo)
        if (attackGroundAsAir > 1.4 && attackGroundAsGround < 1.4) {
            attackGroundAsAir = attackAirAsAir;
        }

        if (!unit.hasTarget())
            unit.getType().isFlyer() ? unit.setSimValue(min(attackAirAsAir, attackGroundAsAir)) : unit.setSimValue(min(attackAirAsGround, attackGroundAsGround));
        else
            unit.getType().isFlyer() ? unit.setSimValue(unitTarget->getType().isFlyer() ? attackAirAsAir : attackGroundAsAir) : unit.setSimValue(unitTarget->getType().isFlyer() ? attackAirAsGround : attackGroundAsGround);
    }
}