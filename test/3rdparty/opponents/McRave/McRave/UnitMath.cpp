#include "McRave.h"

// All calculations involved here can be found here:
// https://docs.google.com/spreadsheets/d/15_2BlFi27EguWciAGbWKCxLLacCnh9QFl1zSkc799Xo

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Math {

    double maxGroundStrength(UnitInfo& unit)
    {
        // HACK: Some hardcoded values
        if (unit.getType() == Terran_Medic)
            return 3.0;
        else if (unit.getGroundDamage() <= 0.0)
            return 0.0;
        else if (unit.getType() == Protoss_Scarab || unit.getType() == Terran_Vulture_Spider_Mine || unit.getType() == Zerg_Egg || unit.getType() == Zerg_Larva)
            return 0.0;

        // Carrier is based on interceptor count
        else if (unit.getType() == Protoss_Carrier) {
            double cnt = 0.0;
            for (auto &i : unit.unit()->getInterceptors()) {
                if (i && !i->exists()) {
                    auto &interceptor = Units::getUnitInfo(i);
                    if (interceptor)
                        cnt += interceptor->getMaxGroundStrength();
                }
            }
            return cnt;
        }

        const auto dps = groundDPS(unit);
        const auto surv = log(survivability(unit));
        const auto eff = grdEffectiveness(unit);
        const auto range = log(unit.getGroundRange() + 64.0);
        return dps * range * surv * eff;
    }

    double visibleGroundStrength(UnitInfo& unit)
    {
        if (unit.unit()->isMaelstrommed() || unit.unit()->isStasised())
            return 0.0;
        return unit.getPercentTotal() * unit.getMaxGroundStrength();
    }

    double maxAirStrength(UnitInfo& unit)
    {
        // HACK: Some hardcoded values
        if (unit.getType() == Terran_Medic)
            return 3.0;
        else if (unit.getAirDamage() <= 0.0)
            return 0.0;

        // Carrier is based on interceptor count
        if (unit.getType() == Protoss_Carrier) {
            double cnt = 0.0;
            for (auto &i : unit.unit()->getInterceptors()) {
                if (i && !i->exists()) {
                    auto &interceptor = Units::getUnitInfo(i);
                    if (interceptor)
                        cnt += interceptor->getMaxAirStrength();
                }
            }
            return cnt;
        }

        const auto dps = airDPS(unit);
        const auto surv = log(survivability(unit));
        const auto eff = airEffectiveness(unit);
        const auto range = log(unit.getAirRange() + 64.0);
        return dps * range * surv * eff;
    }

    double visibleAirStrength(UnitInfo& unit)
    {
        if (unit.unit()->isMaelstrommed() || unit.unit()->isStasised())
            return 0.0;
        return unit.getPercentTotal() * unit.getMaxAirStrength();
    }

    double priority(UnitInfo& unit)
    {
        // According to sheet linked above, these are the maximum values for normalizing
        const auto maxGrdDps = 2.083;
        const auto maxAirDps = 1.875;
        const auto maxCost = 69.589;
        const auto maxSurv = 128.311;
        auto bonus = 1.0;

        if (unit.isMarkedForDeath())
            return 5.0;

        // If target is an egg, larva, scarab or spell
        if (unit.getType() == UnitTypes::Zerg_Egg || unit.getType() == UnitTypes::Zerg_Larva || unit.getType() == UnitTypes::Protoss_Scarab || unit.getType().isSpell())
            return 0.0;

        // Bunch of priority hacks
        if (unit.getType() == Terran_Vulture_Spider_Mine || unit.getType() == Terran_Science_Vessel || unit.getType() == Protoss_Arbiter || unit.getType() == Protoss_Carrier)
            return 15.0;
        if (Strategy::enemyProxy() && unit.getType() == Protoss_Pylon)
            return Grids::getEGroundThreat(unit.getPosition()) == 0.0f ? 5.0 : 1.0;

        // HACK: Kill neutrals blocking geysers for Sparkle
        if (unit.getTilePosition().isValid()) {
            const auto area = mapBWEM.GetArea(unit.getTilePosition());
            if (area && Terrain::isInAllyTerritory(unit.getTilePosition())) {
                for (auto &gas : area->Geysers()) {
                    if (gas->TopLeft().getDistance(unit.getTilePosition()) < 2 && !unit.unit()->isInvincible())
                        return 10.0;
                }
            }
        }

        auto ff = unit.canAttackGround() || unit.canAttackAir() || !unit.getType().isBuilding() ? 1.00 : 0;
        auto dps = ff + max(groundDPS(unit) / maxGrdDps, airDPS(unit) / maxAirDps);
        auto cost = relativeCost(unit) / maxCost;
        auto surv = survivability(unit) / maxSurv;
        return clamp(bonus * (dps * cost / surv), 0.001, 9999.99);
    }

    double relativeCost(UnitInfo& unit)
    {
        double mineral, gas;

        if (unit.getType() == Protoss_Archon)
            mineral = 100.0, gas = 300.0;
        else if (unit.getType() == Protoss_Dark_Archon)
            mineral = 250.0, gas = 200.0;
        else if (unit.getType() == Zerg_Sunken_Colony || unit.getType() == Zerg_Spore_Colony)
            mineral = 175.0, gas = 0.0;
        else
            mineral = unit.getType().mineralPrice(), gas = unit.getType().gasPrice();

        auto supply = (unit.canAttackAir() || unit.canAttackGround() || unit.getType().supplyRequired() > 0) ? max(double(unit.getType().supplyRequired()), 2.0) : 0.5;

        return max(log((mineral * 0.33) + (gas * 0.66)) * supply, 5.00);
    }

    double groundDPS(UnitInfo& unit)
    {
        const auto splash = splashModifier(unit);
        const auto damage = unit.getGroundDamage();
        const auto cooldown = groundCooldown(unit);
        return (damage > 1.0 && cooldown > 0.0) ? splash * damage / cooldown : 0.0;
    }

    double airDPS(UnitInfo& unit)
    {
        const auto splash = splashModifier(unit);
        const auto damage = unit.getAirDamage();
        const auto cooldown = airCooldown(unit);
        return (damage > 1.0 && cooldown > 0.0) ? splash * damage / cooldown : 0.0;
    }

    double groundCooldown(UnitInfo& unit)
    {
        if (unit.getType() == Terran_Bunker)
            return 15.0;
        if (unit.getType() == Protoss_Reaver)
            return 60.0;
        if (unit.getType() == Protoss_High_Templar)
            return 224.0;
        if (unit.getType() == Zerg_Infested_Terran)
            return 500.0;
        if (unit.getType() == Zerg_Zergling && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Adrenal_Glands))
            return 6.0;
        if (unit.getType() == Terran_Marine && unit.getPlayer()->hasResearched(TechTypes::Stim_Packs))
            return 7.5;
        if (unit.getType() == Protoss_Interceptor)
            return 36.0;
        return unit.getType().groundWeapon().damageCooldown();
    }

    double airCooldown(UnitInfo& unit)
    {
        if (unit.getType() == Terran_Bunker)
            return 15.0;
        if (unit.getType() == Protoss_High_Templar)
            return 224.0;
        if (unit.getType() == Zerg_Scourge)
            return 110.0;
        if (unit.getType() == Terran_Marine && unit.getPlayer()->hasResearched(TechTypes::Stim_Packs))
            return 7.5;
        if (unit.getType() == Protoss_Interceptor)
            return 36.0;
        if (unit.getType() == Terran_Valkyrie)
            return 104.0;
        return unit.getType().airWeapon().damageCooldown();
    }

    double splashModifier(UnitInfo& unit)
    {
        if (unit.getType() == Protoss_Archon || unit.getType() == Terran_Firebat || unit.getType() == Protoss_Reaver)
            return 1.25;
        if (unit.getType() == Protoss_High_Templar)
            return 2.00;
        if (unit.getType() == Terran_Siege_Tank_Siege_Mode)
            return 1.50;
        if (unit.getType() == Terran_Valkyrie || unit.getType() == Zerg_Mutalisk)
            return 1.50;
        if (unit.getType() == Zerg_Lurker && Players::ZvT())
            return 2.00;
        return 1.00;
    }

    double grdEffectiveness(UnitInfo& unit)
    {
        auto &sizes = unit.getPlayer() == Broodwar->self() ? Units::getEnemyGrdSizes() : Units::getAllyGrdSizes();
        auto &large = sizes[UnitSizeTypes::Large];
        auto &medium = sizes[UnitSizeTypes::Medium];
        auto &small = sizes[UnitSizeTypes::Small];
        auto total = double(large + medium + small);

        if (total > 0.0) {
            if (unit.getType().groundWeapon().damageType() == DamageTypes::Explosive)
                return ((large*1.0) + (medium*0.75) + (small*0.5)) / total;
            else if (unit.getType().groundWeapon().damageType() == DamageTypes::Concussive)
                return ((large*0.25) + (medium*0.5) + (small*1.0)) / total;
        }
        return 1.0;
    }

    double airEffectiveness(UnitInfo& unit)
    {
        auto &sizes = unit.getPlayer() == Broodwar->self() ? Units::getEnemyAirSizes() : Units::getAllyAirSizes();
        auto &large = sizes[UnitSizeTypes::Large];
        auto &medium = sizes[UnitSizeTypes::Medium];
        auto &small = sizes[UnitSizeTypes::Small];
        auto total = double(large + medium + small);

        if (total > 0.0) {
            if (unit.getType().airWeapon().damageType() == DamageTypes::Explosive)
                return ((large*1.0) + (medium*0.75) + (small*0.5)) / total;
            else if (unit.getType().airWeapon().damageType() == DamageTypes::Concussive)
                return ((large*0.25) + (medium*0.5) + (small*1.0)) / total;
        }
        return 1.0;
    }

    double survivability(UnitInfo& unit)
    {
        const auto armor = [&]() {
            auto upgrades = unit.getPlayer()->getUpgradeLevel(unit.getType().armorUpgrade());
            auto miscUpgrades = (unit.getType() == Zerg_Ultralisk) * 2 * unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Chitinous_Plating);
            return double(1 + upgrades + miscUpgrades);
        };

        const auto avgUnitSpeed = 4.34;
        const auto speed = (unit.getSpeed() + avgUnitSpeed) / avgUnitSpeed;
        const auto health = (double(unit.getType().maxHitPoints() + unit.getType().maxShields())) / 35.0;
        return speed * armor() * health;
    }

    double groundRange(UnitInfo& unit)
    {
        if (unit.getType() == Protoss_Dragoon && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Singularity_Charge))
            return 192.0;
        if ((unit.getType() == Terran_Marine && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::U_238_Shells)) || (unit.getType() == Zerg_Hydralisk && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Grooved_Spines)))
            return 160.0;
        if (unit.getType() == Protoss_High_Templar || unit.getType() == Zerg_Defiler)
            return 288.0;
        if (unit.getType() == Protoss_Carrier)
            return 256.0;
        if (unit.getType() == Protoss_Reaver)
            return 256.0;
        if (unit.getType() == Terran_Bunker) {
            if (unit.getPlayer()->getUpgradeLevel(UpgradeTypes::U_238_Shells))
                return 192.0;
            return 160.0;
        }
        if (unit.getType() == Terran_Vulture_Spider_Mine)
            return 88.0;
        if (unit.getType() == Terran_SCV)
            return 15.0;
        if (unit.getType() == Protoss_Probe || unit.getType() == Zerg_Drone)
            return 32.0;
        if (unit.getType() == Zerg_Ultralisk)
            return 48.0;
        if (unit.getType() == Zerg_Lurker)
            return 192.0 - (96.0 * !unit.isBurrowed());
        return double(unit.getType().groundWeapon().maxRange());
    }

    double airRange(UnitInfo& unit)
    {
        if (unit.getType() == Protoss_Dragoon && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Singularity_Charge))
            return 192.0;
        if ((unit.getType() == Terran_Marine && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::U_238_Shells)) || (unit.getType() == Zerg_Hydralisk && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Grooved_Spines)))
            return 160.0;
        if (unit.getType() == Terran_Goliath && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Charon_Boosters))
            return 256.0;
        if (unit.getType() == Protoss_High_Templar || unit.getType() == Zerg_Defiler)
            return 288.0;
        if (unit.getType() == Protoss_Carrier)
            return 256;
        if (unit.getType() == Terran_Bunker) {
            if (unit.getPlayer()->getUpgradeLevel(UpgradeTypes::U_238_Shells))
                return 192.0;
            return 160.0;
        }
        return double(unit.getType().airWeapon().maxRange());
    }

    double groundReach(UnitInfo& unit)
    {
        return unit.getGroundRange() + (unit.getSpeed() * 16.0) + double(unit.getType().width() / 2) + 64.0;
    }

    double airReach(UnitInfo& unit)
    {
        return unit.getAirRange() + (unit.getSpeed() * 16.0) + double(unit.getType().width() / 2) + 64.0;
    }

    double groundDamage(UnitInfo& unit)
    {
        auto attackCount = max(unit.getType().airWeapon().damageFactor(), unit.getType().maxGroundHits());
        auto upLevel = unit.getPlayer()->getUpgradeLevel(unit.getType().groundWeapon().upgradeType());

        // TODO Check Reaver upgrade type functional here or if needed hardcoding
        if (unit.getType() == Protoss_Reaver) {
            if (unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Scarab_Damage))
                return 125.00;
            return 100.00;
        }
        if (unit.getType() == Terran_Bunker)
            return 24.0 + (4.0 * upLevel);
        if (unit.getType() == Terran_Firebat || unit.getType() == Protoss_Zealot)
            return 16.0 + (2.0 * upLevel);
        if (unit.getType() == Protoss_Interceptor)
            return 12.0 + (2.0 * upLevel);
        if (unit.getType() == Protoss_High_Templar)
            return 112.0;
        return attackCount * (unit.getType().groundWeapon().damageAmount() + (unit.getType().groundWeapon().damageBonus() * upLevel));
    }

    double airDamage(UnitInfo& unit)
    {
        auto attackCount = max(unit.getType().airWeapon().damageFactor(), unit.getType().maxAirHits());
        auto upLevel = unit.getPlayer()->getUpgradeLevel(unit.getType().airWeapon().upgradeType());

        if (unit.getType() == Terran_Bunker)
            return 24.0 + (4.0 * upLevel);
        if (unit.getType() == Protoss_Scout)
            return 28.0 + (2.0 * upLevel);
        if (unit.getType() == Protoss_Interceptor)
            return 12.0 + (2.0 * upLevel);
        if (unit.getType() == Terran_Valkyrie)
            return (48.0 * 0.36) + (8.0 * upLevel);
        if (unit.getType() == Protoss_High_Templar)
            return 112.0;
        return attackCount * (unit.getType().airWeapon().damageAmount() + (unit.getType().airWeapon().damageBonus() * upLevel));
    }

    double moveSpeed(UnitInfo& unit)
    {
        double speed = unit.getType().topSpeed();

        if ((unit.getType() == Zerg_Zergling && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Metabolic_Boost)) || (unit.getType() == Zerg_Hydralisk && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Muscular_Augments)) || (unit.getType() == Zerg_Ultralisk && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Anabolic_Synthesis)) || (unit.getType() == Protoss_Shuttle && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Gravitic_Drive)) || (unit.getType() == Protoss_Observer && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Gravitic_Boosters)) || (unit.getType() == Protoss_Zealot && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Leg_Enhancements)) || (unit.getType() == Terran_Vulture && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Ion_Thrusters)))
            return speed * 1.5;
        if (unit.getType() == Zerg_Overlord && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Pneumatized_Carapace))
            return speed * 4.01;
        if (unit.getType() == Protoss_Scout && unit.getPlayer()->getUpgradeLevel(UpgradeTypes::Muscular_Augments))
            return speed * 1.33;
        if (unit.getType().isBuilding())
            return 0.0;
        if (unit.isBurrowed())
            return 0.0;
        return speed;
    }

    int stopAnimationFrames(UnitType unitType) {
        // Attack animation frames below
        if (unitType == Terran_SCV)
            return 2;
        if (unitType == Terran_Marine)
            return 8;
        if (unitType == Terran_Firebat)
            return 8;
        if (unitType == Terran_Ghost)
            return 3;
        if (unitType == Terran_Vulture)
            return 2;
        if (unitType == Terran_Goliath)
            return 1;
        if (unitType == Terran_Siege_Tank_Tank_Mode || unitType == Terran_Siege_Tank_Siege_Mode)
            return 1;
        if (unitType == Terran_Wraith)
            return 2;
        if (unitType == Terran_Battlecruiser)
            return 2;
        if (unitType == Terran_Valkyrie)
            return 40;
        if (unitType == Protoss_Probe)
            return 2;
        if (unitType == Protoss_Zealot)
            return 7;
        if (unitType == Protoss_Dragoon)
            return 9;
        if (unitType == Protoss_Dark_Templar)
            return 9;
        if (unitType == Protoss_Archon)
            return 15;
        if (unitType == Protoss_Reaver)
            return 1;
        if (unitType == Protoss_Scout)
            return 2;
        if (unitType == Protoss_Corsair)
            return 8;
        if (unitType == Protoss_Arbiter)
            return 4;
        if (unitType == Zerg_Drone)
            return 2;
        if (unitType == Zerg_Zergling)
            return 4;
        if (unitType == Zerg_Hydralisk)
            return 3;
        if (unitType == Zerg_Lurker)
            return 2;
        if (unitType == Zerg_Ultralisk)
            return 14;
        if (unitType == Zerg_Mutalisk)
            return 2;
        if (unitType == Zerg_Devourer)
            return 9;
        return 0;
    }

    int realisticMineralCost(UnitType type)
    {
        if (type == Protoss_Archon)
            return 100;
        else if (type == Protoss_Dark_Archon)
            return 250;
        else if (type == Zerg_Sunken_Colony || type == Zerg_Spore_Colony)
            return 175;
        return type.mineralPrice();
    }

    int realisticGasCost(UnitType type)
    {
        if (type == Protoss_Archon)
            return 300;
        else if (type == Protoss_Dark_Archon)
            return 200;
        return type.gasPrice();
    }
}