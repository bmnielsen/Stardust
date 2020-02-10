#include "UnitCluster.h"

#include <fap.h>
#include "Players.h"
#include "Units.h"

namespace
{
    // Cache of unit scores
    int baseScore[BWAPI::UnitTypes::Enum::MAX];
    int scaledScore[BWAPI::UnitTypes::Enum::MAX];

    auto inline makeUnit(const Unit &unit, int target = 0)
    {
        auto weaponType = unit.type;
        switch (unit.type)
        {
            case BWAPI::UnitTypes::Protoss_Carrier:
                weaponType = BWAPI::UnitTypes::Protoss_Interceptor;
                break;
            case BWAPI::UnitTypes::Terran_Bunker:
                weaponType = BWAPI::UnitTypes::Terran_Marine;
                break;
            case BWAPI::UnitTypes::Protoss_Reaver:
                weaponType = BWAPI::UnitTypes::Protoss_Scarab;
                break;
        }

        int groundDamage = Players::weaponDamage(unit.player, weaponType.groundWeapon());
        int airDamage = Players::weaponDamage(unit.player, weaponType.airWeapon());
        if ((unit.burrowed && unit.type != BWAPI::UnitTypes::Zerg_Lurker) ||
            (!unit.burrowed && unit.type == BWAPI::UnitTypes::Zerg_Lurker))
        {
            groundDamage = airDamage = 0;
        }

        return FAP::makeUnit<>()
                .setUnitType(unit.type)
                .setPosition(unit.lastPosition)
                .setHealth(unit.lastHealth)
                .setShields(unit.lastShields)
                .setFlying(unit.isFlying)

                        // For this next section, we have modified FAP to allow taking the upgraded values instead of the upgrade levels
                .setSpeed(Players::unitTopSpeed(unit.player, unit.type))
                .setArmor(Players::unitArmor(unit.player, unit.type))
                .setGroundCooldown(Players::unitCooldown(unit.player, unit.type)
                                   / std::max(unit.type.maxGroundHits() * unit.type.groundWeapon().damageFactor(), 1))
                .setGroundDamage(groundDamage)
                .setGroundMaxRange(Players::weaponRange(unit.player, weaponType.groundWeapon()))
                .setAirDamage(airDamage)

                .setElevation(BWAPI::Broodwar->getGroundHeight(unit.lastPosition.x << 3, unit.lastPosition.y << 3))
                .setAttackerCount(unit.type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 8)
                .setAttackCooldownRemaining(std::max(0, unit.cooldownUntil - BWAPI::Broodwar->getFrameCount()))

                .setSpeedUpgrade(false) // Squares the speed
                .setRangeUpgrade(false) // Squares the ranges
                .setShieldUpgrades(0)

                .setStimmed(unit.stimmedUntil > BWAPI::Broodwar->getFrameCount())
                .setUndetected(unit.undetected)

                .setID(unit.id)
                .setTarget(target)

                .setData({});
    }

    int score(std::vector<FAP::FAPUnit<>> *units)
    {
        int result = 0;
        for (auto &unit : *units)
        {
            result +=
                    baseScore[unit.unitType] + scaledScore[unit.unitType] * (unit.health * 3 + unit.shields) / (unit.maxHealth * 3 + unit.maxShields);
        }

        return result;
    }
}

namespace CombatSim
{
    void initialize()
    {
        for (auto type : BWAPI::UnitTypes::allUnitTypes())
        {
            int score = type.mineralPrice() + type.gasPrice() * 2;
            baseScore[type] = score >> 2U;
            scaledScore[type] = score - baseScore[type];
        }
    }
}

CombatSimResult
UnitCluster::runCombatSim(std::vector<std::pair<BWAPI::Unit, std::shared_ptr<Unit>>> &unitsAndTargets, std::set<std::shared_ptr<Unit>> &targets)
{
    if (unitsAndTargets.empty() || targets.empty())
    {
        return CombatSimResult();
    }

    FAP::FastAPproximation sim;
    for (auto &unitAndTarget : unitsAndTargets)
    {
        auto target = unitAndTarget.second ? unitAndTarget.second->id : 0;
        sim.addIfCombatUnitPlayer1(makeUnit(Units::get(unitAndTarget.first), target));
    }
    for (auto &unit : targets)
    {
        sim.addIfCombatUnitPlayer2(makeUnit(*unit));
    }

    int initialMine = score(sim.getState().first);
    int initialEnemy = score(sim.getState().second);

    for (int i = 0; i < 144; i++)
    {
        sim.simulate(1);

        /*

        auto first = sim.getState().first;
        auto second = sim.getState().second;

        std::ostringstream log;
        log << BWAPI::WalkPosition(center) << " (" << (i+1) << ")" << ": " << score(first) << "," << score(second);
        for (auto unit : *first)
        {
            log << "\n" << unit.unitType << " @ (" << unit.x << "," << unit.y << "): h=" << unit.health << ";s=" << unit.shields << ";cd=" << unit.attackCooldownRemaining << ";dmg=" << unit.groundDamage;
        }
        for (auto unit : *second)
        {
            log << "\n" << unit.unitType << " @ (" << unit.x << "," << unit.y << "): h=" << unit.health << ";s=" << unit.shields << ";cd=" << unit.attackCooldownRemaining << ";dmg=" << unit.groundDamage;
        }
        Log::Debug() << log.str();
         */
    }

    int finalMine = score(sim.getState().first);
    int finalEnemy = score(sim.getState().second);

#if DEBUG_COMBATSIM
    CherryVis::log() << BWAPI::WalkPosition(center)
                     << ": " << initialMine << "," << initialEnemy
                     << "-" << finalMine << "," << finalEnemy;
#endif

    return CombatSimResult(initialMine, initialEnemy, finalMine, finalEnemy);
}