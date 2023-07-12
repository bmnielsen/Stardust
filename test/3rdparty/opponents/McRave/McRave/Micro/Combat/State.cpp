#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Combat::State {

    void updateLocalState(UnitInfo& unit)
    {
        if (!unit.hasSimTarget() || !unit.hasTarget() || unit.getLocalState() != LocalState::None)
            return;

        auto simTarget = unit.getSimTarget().lock();
        auto unitTarget = unit.getTarget().lock();
        const auto distSim = double(Util::boxDistance(unit.getType(), unit.getPosition(), simTarget->getType(), simTarget->getPosition()));
        const auto distTarget = double(Util::boxDistance(unit.getType(), unit.getPosition(), unitTarget->getType(), unitTarget->getPosition()));
        const auto insideRetreatRadius = distSim < unit.getRetreatRadius();
        const auto insideEngageRadius = distTarget < unit.getEngageRadius() && unit.getGlobalState() == GlobalState::Attack;
        const auto atHome = Terrain::inTerritory(PlayerState::Self, unitTarget->getPosition()) && mapBWEM.GetArea(unit.getTilePosition()) == mapBWEM.GetArea(unitTarget->getTilePosition()) && !Players::ZvZ();
        const auto reAlign = (unit.getType() == Zerg_Mutalisk && !unit.canStartAttack() && !unit.isWithinAngle(*unitTarget) && Util::boxDistance(unit.getType(), unit.getPosition(), unitTarget->getType(), unitTarget->getPosition()) <= 64.0);
        const auto winningState = (!atHome || !BuildOrder::isPlayPassive()) && unit.getSimState() == SimState::Win;
        const auto exploringGoal = unit.getGoal().isValid() && unit.getGoalType() == GoalType::Explore && unit.getUnitsInRangeOfThis().empty() && Util::getTime() > Time(4, 00);

        // Regardless of any decision, determine if Unit is in danger and needs to retreat
        if ((Actions::isInDanger(unit, unit.getPosition()) && !unit.isTargetedBySuicide())
            || (Actions::isInDanger(unit, unit.getEngagePosition()) && insideEngageRadius && !unit.isTargetedBySuicide())
            || (unit.isNearSuicide() && unit.isFlying())
            || reAlign) {
            unit.setLocalState(LocalState::Retreat);
        }

        // Regardless of local decision, determine if Unit needs to attack or retreat
        else if (unit.globalEngage()) {
            unit.setLocalState(LocalState::Attack);
        }
        else if (unit.globalRetreat()) {
            unit.setLocalState(LocalState::Retreat);
        }

        // If within local decision range, determine if Unit needs to attack or retreat
        else if ((insideEngageRadius || atHome) && (unit.localEngage() || winningState || exploringGoal)) {
            unit.setLocalState(LocalState::Attack);
        }
        else if ((insideRetreatRadius || atHome) && (!unit.attemptingRunby() || Terrain::inTerritory(PlayerState::Enemy, unit.getPosition())) && (unit.localRetreat() || unit.getSimState() == SimState::Loss)) {
            unit.setLocalState(LocalState::Retreat);
        }

        // Default state
        else
            unit.setLocalState(LocalState::None);
    }

    void updateGlobalState(UnitInfo& unit)
    {
        if (unit.getGlobalState() != GlobalState::None)
            return;

        // Protoss
        if (Broodwar->self()->getRace() == Races::Protoss) {
            if ((!BuildOrder::takeNatural() && Spy::enemyFastExpand())
                || (Spy::enemyProxy() && !Spy::enemyRush())
                || BuildOrder::isRush()
                || unit.getType() == Protoss_Dark_Templar
                || (Players::getVisibleCount(PlayerState::Enemy, Protoss_Dark_Templar) > 0 && com(Protoss_Observer) == 0 && Broodwar->getFrameCount() < 15000))
                unit.setGlobalState(GlobalState::Attack);

            else if (unit.getType().isWorker()
                || (Broodwar->getFrameCount() < 15000 && BuildOrder::isPlayPassive())
                || (unit.getType() == Protoss_Corsair && !BuildOrder::firstReady() && Players::getStrength(PlayerState::Enemy).airToAir > 0.0)
                || (unit.getType() == Protoss_Carrier && com(Protoss_Interceptor) < 16 && !Spy::enemyPressure()))
                unit.setGlobalState(GlobalState::Retreat);
            else
                unit.setGlobalState(GlobalState::Attack);
        }

        // Zerg
        else if (Broodwar->self()->getRace() == Races::Zerg) {
            if (BuildOrder::isRush()
                || Broodwar->getGameType() == GameTypes::Use_Map_Settings
                || unit.attemptingRunby())
                unit.setGlobalState(GlobalState::Attack);
            else if ((Broodwar->getFrameCount() < 15000 && BuildOrder::isPlayPassive() && !unit.getGoal().isValid())
                || (Players::ZvT() && Util::getTime() < Time(12, 00) && Util::getTime() > Time(3, 30) && unit.getType() == Zerg_Zergling && !Spy::enemyGreedy() && (Spy::getEnemyBuild() == "RaxFact" || Spy::enemyWalled() || Players::getCompleteCount(PlayerState::Enemy, Terran_Vulture) > 0))
                || (Players::ZvZ() && Util::getTime() < Time(10, 00) && unit.getType() == Zerg_Zergling && Players::getCompleteCount(PlayerState::Enemy, Zerg_Zergling) > com(Zerg_Zergling))
                || (Players::ZvZ() && Players::getCompleteCount(PlayerState::Enemy, Zerg_Drone) > 0 && !Terrain::getEnemyStartingPosition().isValid() && Util::getTime() < Time(2, 45))
                || (BuildOrder::isProxy() && BuildOrder::isPlayPassive())
                || (BuildOrder::isProxy() && unit.getType() == Zerg_Hydralisk)
                || (unit.getType() == Zerg_Hydralisk && BuildOrder::getCompositionPercentage(Zerg_Lurker) >= 1.00)
                || (unit.getType() == Zerg_Hydralisk && !unit.getGoal().isValid() && (!Players::getPlayerInfo(Broodwar->self())->hasUpgrade(UpgradeTypes::Grooved_Spines) || !Players::getPlayerInfo(Broodwar->self())->hasUpgrade(UpgradeTypes::Muscular_Augments)))
                || (!Players::ZvZ() && unit.isLightAir() && com(Zerg_Mutalisk) < (Stations::getStations(PlayerState::Self).size() >= 2 ? 5 : 3) && total(Zerg_Mutalisk) < 9))
                unit.setGlobalState(GlobalState::Retreat);
            else
                unit.setGlobalState(GlobalState::Attack);
        }

        // Terran
        else if (Broodwar->self()->getRace() == Races::Terran) {
            if (BuildOrder::isPlayPassive() || !BuildOrder::firstReady())
                unit.setGlobalState(GlobalState::Retreat);
            else
                unit.setGlobalState(GlobalState::Attack);
        }
    }

    void update(UnitInfo& unit)
    {
        updateGlobalState(unit);
        updateLocalState(unit);
    }
}