#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Combat::Simulation {

    namespace {
        bool ignoreSim = false;
        double minWinPercent = 0.6;
        double maxWinPercent = 1.2;
        double minThreshold = 0.0;
        double maxThreshold = 0.0;
    }

    void updateSimulation(UnitInfo& unit)
    {
        if (unit.getEngDist() == DBL_MAX || !unit.hasTarget()) {
            unit.setSimState(SimState::Loss);
            unit.setSimValue(0.0);
            return;
        }

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

        auto belowGrdtoGrdLimit = false;
        auto belowGrdtoAirLimit = false;
        auto belowAirtoAirLimit = false;
        auto belowAirtoGrdLimit = false;
        auto engagedAlreadyOffset = unit.getSimState() == SimState::Win ? 0.2 : 0.0;
        auto unitTarget = unit.getTarget().lock();

        // Check if any allied unit is below the limit to synchronize sim values
        for (auto &a : Units::getUnits(PlayerState::Self)) {
            UnitInfo &self = *a;

            if (self.hasTarget()) {
                auto selfTarget = self.getTarget().lock();
                if (selfTarget == unitTarget && self.getSimValue() <= minThreshold && self.getSimValue() != 0.0) {
                    self.isFlying() ?
                        (selfTarget->isFlying() ? belowAirtoAirLimit = true : belowAirtoGrdLimit = true) :
                        (selfTarget->isFlying() ? belowGrdtoAirLimit = true : belowGrdtoGrdLimit = true);
                }
            }
        }
        for (auto &a : Units::getUnits(PlayerState::Ally)) {
            UnitInfo &ally = *a;

            if (ally.hasTarget()) {
                auto allyTarget = ally.getTarget().lock();
                if (ally.getTarget() == unit.getTarget() && ally.getSimValue() <= minThreshold && ally.getSimValue() != 0.0) {
                    ally.isFlying() ?
                        (allyTarget->isFlying() ? belowAirtoAirLimit = true : belowAirtoGrdLimit = true) :
                        (allyTarget->isFlying() ? belowGrdtoAirLimit = true : belowGrdtoGrdLimit = true);
                }
            }
        }

        // If above/below thresholds, it's a sim win/loss
        if (unit.getSimValue() >= maxThreshold - engagedAlreadyOffset)
            unit.setSimState(SimState::Win);
        else if (unit.getSimValue() < minThreshold - engagedAlreadyOffset || (unit.getSimState() == SimState::None && unit.getSimValue() < maxThreshold))
            unit.setSimState(SimState::Loss);

        // Check for hardcoded directional losses
        if (unit.getSimValue() < maxThreshold - engagedAlreadyOffset) {
            if (unit.isFlying()) {
                if (unitTarget->isFlying() && belowAirtoAirLimit)
                    unit.setSimState(SimState::Loss);
                else if (!unitTarget->isFlying() && belowAirtoGrdLimit)
                    unit.setSimState(SimState::Loss);
            }
            else {
                if (unitTarget->isFlying() && belowGrdtoAirLimit)
                    unit.setSimState(SimState::Loss);
                else if (!unitTarget->isFlying() && belowGrdtoGrdLimit)
                    unit.setSimState(SimState::Loss);
            }
        }
    }

    void updateThresholds(UnitInfo& unit)
    {
        // P
        if (Players::PvP()) {
            minWinPercent = 0.8;
            maxWinPercent = 1.2;
        }
        if (Players::PvZ()) {
            minWinPercent = 0.6;
            maxWinPercent = 1.2;
        }
        if (Players::PvT()) {
            minWinPercent = 0.6;
            maxWinPercent = 1.0;
        }

        // Z
        if (Players::ZvP()) {
            minWinPercent = 1.0;
            maxWinPercent = 1.4;
        }
        if (Players::ZvZ()) {
            minWinPercent = 0.8;
            maxWinPercent = 1.4;
        }
        if (Players::ZvT()) {
            minWinPercent = 1.0;
            maxWinPercent = 1.4;
        }

        minThreshold = minWinPercent;
        maxThreshold = maxWinPercent;
    }

    void update(UnitInfo& unit)
    {
        updateThresholds(unit);
        Horizon::simulate(unit);
        updateSimulation(unit);
    }
}