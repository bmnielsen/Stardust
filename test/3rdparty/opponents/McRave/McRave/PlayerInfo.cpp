#include "McRave.h"

using namespace std;
using namespace BWAPI;
using namespace UnitTypes;

namespace McRave
{
    namespace {
        map<Unit, UnitType> actualEggType; /// BWAPI issue #850
    }

    void PlayerInfo::update()
    {
        // Store any upgrades this player has
        for (auto &upgrade : BWAPI::UpgradeTypes::allUpgradeTypes()) {
            if (thisPlayer->getUpgradeLevel(upgrade) > 0)
                playerUpgrades.insert(upgrade);
        }

        // Store any tech this player has
        for (auto &tech : BWAPI::TechTypes::allTechTypes()) {
            if (thisPlayer->hasResearched(tech))
                playerTechs.insert(tech);
        }

        // Update player units
        raceSupply.clear();
        pStrength.clear();
        visibleTypeCounts.clear();
        completeTypeCounts.clear();
        for (auto &u : units) {
            auto &unit = *u;
            auto type = unit.getType() == UnitTypes::Zerg_Egg ? unit.unit()->getBuildType() : unit.getType();

            /// BWAPI issue #850
            if (type == UnitTypes::None && actualEggType.find(unit.unit()) != actualEggType.end())
                type = actualEggType[unit.unit()];
            if (unit.getType() == UnitTypes::Zerg_Egg && unit.unit()->getBuildType() != UnitTypes::None) {
                actualEggType[unit.unit()] = unit.unit()->getBuildType();
                type = unit.unit()->getBuildType();
            }

            // Supply
            raceSupply[type.getRace()] += type.supplyRequired();
            raceSupply[type.getRace()] += type == Zerg_Zergling || type == Zerg_Scourge;

            // All supply
            raceSupply[Races::None] += type.supplyRequired();
            raceSupply[Races::None] += type == Zerg_Zergling || type == Zerg_Scourge;

            // Targets
            unit.getTargetedBy().clear();
            unit.setTarget(nullptr);
            unit.borrowedPath = false;

            // Counts
            int eggOffset = int(isSelf() && !unit.isCompleted() && (type == Zerg_Zergling || type == Zerg_Scourge));
            if (type != UnitTypes::None && type.isValid()) {
                visibleTypeCounts[type] += 1 + eggOffset;
                if (unit.isCompleted())
                    completeTypeCounts[type] += 1;
            }

            // Strength
            if ((type.isWorker() && unit.getRole() != Role::Combat)
                || (unit.unit()->exists() && !type.isBuilding() && !unit.unit()->isCompleted())
                || (unit.unit()->isMorphing()))
                continue;

            if (type.isBuilding()) {
                pStrength.airDefense += unit.getVisibleAirStrength();
                pStrength.groundDefense += unit.getVisibleGroundStrength();
            }
            else if (type.isFlyer() && !unit.isSpellcaster() && !type.isWorker()) {
                pStrength.airToAir += unit.getVisibleAirStrength();
                pStrength.airToGround += unit.getVisibleGroundStrength();
            }
            else if (!unit.isSpellcaster() && !type.isWorker()) {
                pStrength.groundToAir += unit.getVisibleAirStrength();
                pStrength.groundToGround += unit.getVisibleGroundStrength();
            }
        }

        // Supply has to be an even number and is rounded up - TODO: Check
        for (auto &[race, supply] : raceSupply) {
            if (supply % 2 == 1)
                supply++;
        }

        // Set current allied status
        if (thisPlayer->getID() == BWAPI::Broodwar->self()->getID())
            pState = PlayerState::Self;
        else if (thisPlayer->isEnemy(BWAPI::Broodwar->self()))
            pState = PlayerState::Enemy;
        else if (thisPlayer->isAlly(BWAPI::Broodwar->self()))
            pState = PlayerState::Ally;
        else
            pState = PlayerState::Neutral;

        auto race = thisPlayer->isNeutral() || !alive ? BWAPI::Races::None : thisPlayer->getRace(); // BWAPI returns Zerg for neutral race
        currentRace = race;
    }
}