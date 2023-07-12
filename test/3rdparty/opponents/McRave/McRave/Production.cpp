#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Production {

    namespace {
        map <Unit, UnitType> idleProduction;
        map <Unit, TechType> idleTech;
        map <Unit, UpgradeType> idleUpgrade;
        map <UnitType, int> trainedThisFrame;
        int reservedMineral, reservedGas;
        int lastTrainFrame = -999;

        void reset()
        {
            trainedThisFrame.clear();
            reservedMineral = 0;
            reservedGas = 0;
        }

        bool haveOrUpgrading(UpgradeType upgrade, int level) {
            return ((Broodwar->self()->isUpgrading(upgrade) && Broodwar->self()->getUpgradeLevel(upgrade) == level - 1) || Broodwar->self()->getUpgradeLevel(upgrade) >= level);
        }

        bool isAffordable(UnitType unit)
        {
            if (unit == Zerg_Overlord)
                return Broodwar->self()->minerals() >= unit.mineralPrice() + Planning::getPlannedMineral();

            auto selfMineral            = Broodwar->self()->minerals();
            auto selfGas                = Broodwar->self()->gas();
            auto selfSupply             = Players::getSupply(PlayerState::Self, unit.getRace());

            for (auto &[type, cnt] : trainedThisFrame) {
                selfMineral            -= type.mineralPrice() * cnt;
                selfGas                -= type.gasPrice() * cnt;
                selfSupply             += type.supplyRequired() * cnt;
            }

            auto mineralReserve         = int(!BuildOrder::isTechUnit(unit) || !idleTech.empty() || !idleUpgrade.empty()) * reservedMineral;
            auto gasReserve             = int(!BuildOrder::isTechUnit(unit) || !idleTech.empty() || !idleUpgrade.empty()) * reservedGas;
            auto mineralAffordable      = unit.mineralPrice() == 0 || (selfMineral >= unit.mineralPrice() + Planning::getPlannedMineral() + mineralReserve);
            auto gasAffordable          = unit.gasPrice() == 0 || (selfGas >= unit.gasPrice() + Planning::getPlannedGas() + gasReserve);
            auto supplyAffordable       = unit.supplyRequired() == 0 || (selfSupply + unit.supplyRequired() <= Broodwar->self()->supplyTotal());

            return mineralAffordable && gasAffordable && supplyAffordable;
        }

        bool isAffordable(TechType tech)
        {
            return Broodwar->self()->minerals() >= tech.mineralPrice() && Broodwar->self()->gas() >= tech.gasPrice();
        }

        bool isAffordable(UpgradeType upgrade)
        {
            auto mineralCost = upgrade.mineralPrice() + (upgrade.mineralPriceFactor() * Broodwar->self()->getUpgradeLevel(upgrade));
            auto gasCost = upgrade.gasPrice() + (upgrade.gasPriceFactor() * Broodwar->self()->getUpgradeLevel(upgrade));
            return Broodwar->self()->minerals() >= mineralCost && Broodwar->self()->gas() >= gasCost;
        }

        bool isCreateable(Unit building, UnitType unit)
        {
            if (!BuildOrder::isUnitUnlocked(unit)
                || BuildOrder::getCompositionPercentage(unit) <= 0.0)
                return false;

            switch (unit)
            {
            case Enum::Protoss_Probe:
                return true;

                // Gateway Units
            case Enum::Protoss_Zealot:
                return true;
            case Enum::Protoss_Dragoon:
                return com(Protoss_Cybernetics_Core) > 0;
            case Enum::Protoss_Dark_Templar:
                return com(Protoss_Templar_Archives) > 0;
            case Enum::Protoss_High_Templar:
                return com(Protoss_Templar_Archives) > 0;

                // Robo Units
            case Enum::Protoss_Shuttle:
                return true;
            case Enum::Protoss_Reaver:
                return com(Protoss_Robotics_Support_Bay) > 0;
            case Enum::Protoss_Observer:
                return com(Protoss_Observatory) > 0;

                // Stargate Units
            case Enum::Protoss_Corsair:
                return true;
            case Enum::Protoss_Scout:
                return true;
            case Enum::Protoss_Carrier:
                return com(Protoss_Fleet_Beacon) > 0;
            case Enum::Protoss_Arbiter:
                return com(Protoss_Arbiter_Tribunal) > 0;

            case Enum::Terran_SCV:
                return true;

                // Barracks Units
            case Enum::Terran_Marine:
                return true;
            case Enum::Terran_Firebat:
                return com(Terran_Academy) > 0;
            case Enum::Terran_Medic:
                return com(Terran_Academy) > 0;
            case Enum::Terran_Ghost:
                return com(Terran_Covert_Ops) > 0;
            case Enum::Terran_Nuclear_Missile:
                return com(Terran_Covert_Ops) > 0;

                // Factory Units
            case Enum::Terran_Vulture:
                return true;
            case Enum::Terran_Siege_Tank_Tank_Mode:
                return building->getAddon() != nullptr ? true : false;
            case Enum::Terran_Goliath:
                return (com(Terran_Armory) > 0);

                // Starport Units
            case Enum::Terran_Wraith:
                return true;
            case Enum::Terran_Valkyrie:
                return (com(Terran_Armory) > 0 && building->getAddon() != nullptr) ? true : false;
            case Enum::Terran_Battlecruiser:
                return (com(Terran_Physics_Lab) && building->getAddon() != nullptr) ? true : false;
            case Enum::Terran_Science_Vessel:
                return (com(Terran_Science_Facility) > 0 && building->getAddon() != nullptr) ? true : false;
            case Enum::Terran_Dropship:
                return building->getAddon() != nullptr ? true : false;

                // Zerg Units
            case Enum::Zerg_Drone:
                return true;
            case Enum::Zerg_Zergling:
                return com(Zerg_Spawning_Pool) > 0;
            case Enum::Zerg_Hydralisk:
                return com(Zerg_Hydralisk_Den) > 0;
            case Enum::Zerg_Mutalisk:
                return com(Zerg_Spire) > 0 || com(Zerg_Greater_Spire) > 0;
            case Enum::Zerg_Scourge:
                return com(Zerg_Spire) > 0 || com(Zerg_Greater_Spire) > 0;
            case Enum::Zerg_Defiler:
                return com(Zerg_Defiler_Mound) > 0;
            case Enum::Zerg_Ultralisk:
                return com(Zerg_Ultralisk_Cavern) > 0;
            }
            return false;
        }

        bool isCreateable(Unit building, UpgradeType upgrade)
        {
            auto geyserType = Broodwar->self()->getRace().getRefinery();
            if (upgrade.gasPrice() > 0 && com(geyserType) == 0)
                return false;

            // First upgrade check
            if (upgrade == BuildOrder::getFirstUpgrade() && Broodwar->self()->getUpgradeLevel(upgrade) == 0 && !Broodwar->self()->isUpgrading(upgrade))
                return true;

            // Upgrades that require a building
            if (upgrade == UpgradeTypes::Adrenal_Glands && Broodwar->self()->getUpgradeLevel(upgrade) == 0 && !Broodwar->self()->isUpgrading(upgrade))
                return com(Zerg_Hive);

            // Armor/Weapon upgrades
            if (upgrade.maxRepeats() >= 3) {
                if (upgrade.getRace() == Races::Zerg && ((Broodwar->self()->getUpgradeLevel(upgrade) == 1 && com(Zerg_Lair) == 0 && com(Zerg_Hive) == 0) || (Broodwar->self()->getUpgradeLevel(upgrade) == 2 && com(Zerg_Hive) == 0)))
                    return false;
                if (upgrade.getRace() == Races::Protoss && Broodwar->self()->getUpgradeLevel(upgrade) >= 1 && com(Protoss_Templar_Archives) == 0)
                    return false;
                if (upgrade.getRace() == Races::Terran && Broodwar->self()->getUpgradeLevel(upgrade) >= 1 && com(Terran_Science_Facility) == 0)
                    return false;
            }

            for (auto &unit : upgrade.whatUses()) {
                if (BuildOrder::isUnitUnlocked(unit) && Broodwar->self()->getUpgradeLevel(upgrade) != upgrade.maxRepeats() && !Broodwar->self()->isUpgrading(upgrade))
                    return true;
            }
            return false;
        }

        bool isCreateable(Unit building, TechType tech)
        {
            // First tech check
            if (tech == BuildOrder::getFirstTech() && !Broodwar->self()->hasResearched(tech) && !Broodwar->self()->isResearching(tech))
                return true;

            // Avoid researching Burrow with a Lair/Hive
            if (tech == TechTypes::Burrowing && building->getType() != Zerg_Hatchery)
                return false;

            // Tech that require a building
            if (tech == TechTypes::Lurker_Aspect && !Broodwar->self()->hasResearched(tech) && !Broodwar->self()->isResearching(tech))
                return (com(Zerg_Lair) > 0 || com(Zerg_Hive) > 0) && BuildOrder::isUnitUnlocked(Zerg_Lurker);

            for (auto &unit : tech.whatUses()) {
                if (BuildOrder::isUnitUnlocked(unit) && !Broodwar->self()->hasResearched(tech) && !Broodwar->self()->isResearching(tech))
                    return true;
            }
            return false;
        }

        bool isSuitable(UnitType unit)
        {
            if (unit.isWorker())
                return vis(unit) < 70 && (!Resources::isMineralSaturated() || !Resources::isGasSaturated());

            if (unit.getRace() == Races::Zerg) {
                if (unit == Zerg_Defiler)
                    return vis(unit) < 4;
                return true;
            }

            // Determine whether we want reavers or shuttles
            bool needReavers = false;
            bool needShuttles = false;
            if (!Spy::enemyInvis() && BuildOrder::isTechUnit(Protoss_Reaver)) {
                if ((Terrain::isIslandMap() && vis(unit) < 2 * vis(Protoss_Nexus))
                    || vis(Protoss_Shuttle) == 0
                    || vis(Protoss_Reaver) > vis(Protoss_Shuttle) * 2
                    || vis(Protoss_High_Templar) > vis(Protoss_Shuttle) * 4)
                    needShuttles = true;
                if (!Terrain::isIslandMap() || (vis(Protoss_Reaver) <= (vis(Protoss_Shuttle) * 2)))
                    needReavers = true;
            }

            // Determine our templar caps
            auto htCap = min(2 * (Players::getSupply(PlayerState::Self, Races::Protoss) / 100), Players::vP() ? 4 : 8);

            switch (unit)
            {
                // Gateway Units
            case Protoss_Zealot:
                return true;
            case Protoss_Dragoon:
                return true;
            case Protoss_Dark_Templar:
                return vis(unit) < 4;
            case Protoss_High_Templar:
                return (vis(unit) < 2 + (Players::getSupply(PlayerState::Self, Races::Protoss) / 200) && vis(unit) <= com(unit) + 2) || Production::scoreUnit(UnitTypes::Protoss_Archon) > Production::scoreUnit(UnitTypes::Protoss_High_Templar);

                // Robo Units
            case Protoss_Shuttle:
                return needShuttles;
            case Protoss_Reaver:
                return needReavers;
            case Protoss_Observer:
                return (Util::getTime() > Time(10, 0) || Spy::enemyInvis()) ? vis(unit) < 1 + (Players::getSupply(PlayerState::Self, Races::Protoss) / 100) : vis(unit) < 1;

                // Stargate Units
            case Protoss_Corsair:
                return vis(unit) < (10 + (Terrain::isIslandMap() * 10));
            case Protoss_Scout:
                return true;
            case Protoss_Carrier:
                return true;
            case Protoss_Arbiter:
                return (vis(unit) < 8 && (Broodwar->self()->isUpgrading(UpgradeTypes::Khaydarin_Core) || Broodwar->self()->getUpgradeLevel(UpgradeTypes::Khaydarin_Core)));

                // Barracks Units
            case Terran_Marine:
                return true;
            case Terran_Firebat:
                return true;
            case Terran_Medic:
                return com(unit) * 4 < com(Terran_Marine);
            case Terran_Ghost:
                return false;
            case Terran_Nuclear_Missile:
                return false;

                // Factory Units
            case Terran_Vulture:
                return true;
            case Terran_Siege_Tank_Tank_Mode:
                return true;
            case Terran_Goliath:
                return true;

                // Starport Units
            case Terran_Wraith:
                return false;
            case Terran_Valkyrie:
                return false;
            case Terran_Battlecruiser:
                return true;
            case Terran_Science_Vessel:
                return vis(unit) < 6;
            case Terran_Dropship:
                return vis(unit) <= 0;
            }
            return false;
        }

        bool isSuitable(UpgradeType upgrade)
        {
            using namespace UpgradeTypes;

            // Allow first upgrade
            if (upgrade == BuildOrder::getFirstUpgrade() && !BuildOrder::firstReady())
                return true;

            // Don't upgrade anything in opener if nothing is chosen
            if (BuildOrder::getFirstUpgrade() == UpgradeTypes::None && BuildOrder::isOpener())
                return false;

            // If this is a specific unit upgrade, check if it's unlocked
            if (upgrade.whatUses().size() == 1) {
                for (auto &unit : upgrade.whatUses()) {
                    if (!BuildOrder::isUnitUnlocked(unit))
                        return false;
                }
            }

            // If this isn't the first upgrade and we don't have our first tech/upgrade
            if (upgrade != BuildOrder::getFirstUpgrade()) {
                if (BuildOrder::getFirstUpgrade() != UpgradeTypes::None && Broodwar->self()->getUpgradeLevel(BuildOrder::getFirstUpgrade()) <= 0 && !Broodwar->self()->isUpgrading(BuildOrder::getFirstUpgrade()))
                    return false;
                if (BuildOrder::getFirstTech() != TechTypes::None && !Broodwar->self()->hasResearched(BuildOrder::getFirstTech()) && !Broodwar->self()->isResearching(BuildOrder::getFirstTech()))
                    return false;
            }

            // If we're playing Protoss, check Protoss upgrades
            if (Broodwar->self()->getRace() == Races::Protoss) {
                switch (upgrade) {

                    // Energy upgrades
                case Khaydarin_Amulet:
                    return (vis(Protoss_Assimilator) >= 4 && Broodwar->self()->hasResearched(TechTypes::Psionic_Storm) && Broodwar->self()->gas() >= 750);
                case Khaydarin_Core:
                    return true;

                    // Range upgrades
                case Singularity_Charge:
                    return vis(Protoss_Dragoon) >= 1;

                    // Sight upgrades
                case Apial_Sensors:
                    return (Broodwar->self()->minerals() > 1500 && Broodwar->self()->gas() > 1000);
                case Sensor_Array:
                    return (Broodwar->self()->minerals() > 1500 && Broodwar->self()->gas() > 1000);

                    // Capacity upgrades
                case Carrier_Capacity:
                    return vis(Protoss_Carrier) >= 2;
                case Reaver_Capacity:
                    return (Broodwar->self()->minerals() > 1500 && Broodwar->self()->gas() > 1000);
                case Scarab_Damage:
                    return (Broodwar->self()->minerals() > 1500 && Broodwar->self()->gas() > 1000);

                    // Speed upgrades
                case Gravitic_Drive:
                    return vis(Protoss_Shuttle) > 0 && (vis(Protoss_High_Templar) > 0 || com(Protoss_Reaver) >= 2);
                case Gravitic_Thrusters:
                    return vis(Protoss_Scout) > 0;
                case Gravitic_Boosters:
                    return (Broodwar->self()->minerals() > 1500 && Broodwar->self()->gas() > 1000);
                case Leg_Enhancements:
                    return (vis(Protoss_Nexus) >= 3) || Players::PvZ();

                    // Ground unit upgrades
                case Protoss_Ground_Weapons:
                    return !Terrain::isIslandMap() && (Players::getSupply(PlayerState::Self, Races::Protoss) > 300 || Players::vZ());
                case Protoss_Ground_Armor:
                    return !Terrain::isIslandMap() && (Broodwar->self()->getUpgradeLevel(Protoss_Ground_Weapons) > Broodwar->self()->getUpgradeLevel(Protoss_Ground_Armor) || Broodwar->self()->isUpgrading(Protoss_Ground_Weapons));
                case Protoss_Plasma_Shields:
                    return haveOrUpgrading(Protoss_Ground_Weapons, 3) && haveOrUpgrading(Protoss_Ground_Armor, 3);

                    // Air unit upgrades
                case Protoss_Air_Weapons:
                    return (vis(Protoss_Corsair) > 0 || vis(Protoss_Scout) > 0 || (vis(Protoss_Stargate) > 0 && BuildOrder::isTechUnit(Protoss_Carrier) && Players::vT()));
                case Protoss_Air_Armor:
                    return Broodwar->self()->getUpgradeLevel(Protoss_Air_Weapons) > Broodwar->self()->getUpgradeLevel(Protoss_Air_Armor);
                }
            }

            else if (Broodwar->self()->getRace() == Races::Terran) {
                switch (upgrade) {

                    // Speed upgrades
                case Ion_Thrusters:
                    return true;

                    // Range upgrades
                case Charon_Boosters:
                    return vis(Terran_Goliath) > 0;
                case U_238_Shells:
                    return Broodwar->self()->hasResearched(TechTypes::Stim_Packs);

                    // Bio upgrades
                case Terran_Infantry_Weapons:
                    return true;
                case Terran_Infantry_Armor:
                    return (Broodwar->self()->getUpgradeLevel(Terran_Infantry_Weapons) > Broodwar->self()->getUpgradeLevel(Terran_Infantry_Armor) || Broodwar->self()->isUpgrading(Terran_Infantry_Weapons));

                    // Mech upgrades
                case Terran_Vehicle_Weapons:
                    return (Players::getStrength(PlayerState::Self).groundToGround > 20.0);
                case Terran_Vehicle_Plating:
                    return (Broodwar->self()->getUpgradeLevel(Terran_Vehicle_Weapons) > Broodwar->self()->getUpgradeLevel(Terran_Vehicle_Plating) || Broodwar->self()->isUpgrading(Terran_Vehicle_Weapons));
                case Terran_Ship_Weapons:
                    return (Players::getStrength(PlayerState::Self).airToAir > 20.0);
                case Terran_Ship_Plating:
                    return (Broodwar->self()->getUpgradeLevel(Terran_Ship_Weapons) > Broodwar->self()->getUpgradeLevel(Terran_Ship_Plating) || Broodwar->self()->isUpgrading(Terran_Ship_Weapons));
                }
            }

            else if (Broodwar->self()->getRace() == Races::Zerg) {
                switch (upgrade)
                {
                    // Speed upgrades
                case Metabolic_Boost:
                    return vis(Zerg_Zergling) >= 20 || total(Zerg_Hive) > 0 || total(Zerg_Defiler_Mound) > 0;
                case Muscular_Augments:
                    return (BuildOrder::isTechUnit(Zerg_Hydralisk) && Players::ZvP()) || Broodwar->self()->getUpgradeLevel(Grooved_Spines) > 0;
                case Pneumatized_Carapace:
                    return (Players::ZvT() && Spy::getEnemyTransition() == "2PortWraith")
                        || (Players::ZvP() && Players::getStrength(PlayerState::Enemy).airToAir > 0 && Players::getSupply(PlayerState::Self, Races::Zerg) >= 160)
                        || (Spy::enemyInvis() && (BuildOrder::isTechUnit(Zerg_Hydralisk) || BuildOrder::isTechUnit(Zerg_Ultralisk)))
                        || (Players::getSupply(PlayerState::Self, Races::Zerg) >= 200);
                case Anabolic_Synthesis:
                    return Players::getTotalCount(PlayerState::Enemy, Terran_Marine) < 20 || Broodwar->self()->getUpgradeLevel(Chitinous_Plating) > 0;

                    // Range upgrades
                case Grooved_Spines:
                    return (BuildOrder::isTechUnit(Zerg_Hydralisk) && !Players::ZvP()) || Broodwar->self()->getUpgradeLevel(Muscular_Augments) > 0;

                    // Other upgrades
                case Chitinous_Plating:
                    return Players::getTotalCount(PlayerState::Enemy, Terran_Marine) >= 20 || Broodwar->self()->getUpgradeLevel(Anabolic_Synthesis) > 0;
                case Adrenal_Glands:
                    return Broodwar->self()->getUpgradeLevel(Metabolic_Boost) > 0;

                    // Ground unit upgrades
                case Zerg_Melee_Attacks:
                    return (BuildOrder::getCompositionPercentage(Zerg_Zergling) > 0.0 || BuildOrder::getCompositionPercentage(Zerg_Ultralisk) > 0.0) && Players::getSupply(PlayerState::Self, Races::Zerg) > 160 && (Broodwar->self()->getUpgradeLevel(Zerg_Carapace) > Broodwar->self()->getUpgradeLevel(Zerg_Melee_Attacks) || Broodwar->self()->isUpgrading(Zerg_Carapace));
                case Zerg_Missile_Attacks:
                    return (BuildOrder::getCompositionPercentage(Zerg_Hydralisk) > 0.0 || BuildOrder::getCompositionPercentage(Zerg_Lurker) > 0.0) && (Broodwar->self()->getUpgradeLevel(Zerg_Carapace) > Broodwar->self()->getUpgradeLevel(Zerg_Missile_Attacks) || Broodwar->self()->isUpgrading(Zerg_Carapace));
                case Zerg_Carapace:
                    return (BuildOrder::getTechUnit() == Zerg_Ultralisk || BuildOrder::getCompositionPercentage(Zerg_Hydralisk) > 0.0 || BuildOrder::getCompositionPercentage(Zerg_Zergling) > 0.0 || BuildOrder::getCompositionPercentage(Zerg_Ultralisk) > 0.0) && Players::getSupply(PlayerState::Self, Races::Zerg) > 100;

                    // Air unit upgrades
                case Zerg_Flyer_Attacks:
                    if (Players::ZvP() && Players::getVisibleCount(PlayerState::Enemy, Protoss_Corsair) < 2)
                        return false;
                    if (Players::ZvZ() && int(Stations::getStations(PlayerState::Self).size()) < 2)
                        return false;
                    return BuildOrder::getCompositionPercentage(Zerg_Mutalisk) > 0.0 && (Broodwar->self()->getUpgradeLevel(Zerg_Flyer_Carapace) > Broodwar->self()->getUpgradeLevel(Zerg_Flyer_Attacks) + 1 || Broodwar->self()->isUpgrading(Zerg_Flyer_Carapace));
                case Zerg_Flyer_Carapace:
                    if (Players::ZvZ() && int(Stations::getStations(PlayerState::Self).size()) < 2)
                        return false;
                    return BuildOrder::getCompositionPercentage(Zerg_Mutalisk) > 0.0 && total(Zerg_Mutalisk) >= 12 && Stations::getStations(PlayerState::Self).size() >= 3;
                }
            }
            return false;
        }

        bool isSuitable(TechType tech)
        {
            using namespace TechTypes;

            // Allow first tech
            if (tech == BuildOrder::getFirstTech() && !BuildOrder::firstReady())
                return true;

            // If this is a specific unit tech, check if it's unlocked
            if (tech != BuildOrder::getFirstTech() && tech.whatUses().size() == 1) {
                for (auto &unit : tech.whatUses()) {
                    if (!BuildOrder::isUnitUnlocked(unit))
                        return false;
                }
            }

            // If this isn't the first tech and we don't have our first tech/upgrade
            if (tech != BuildOrder::getFirstTech()) {
                if (BuildOrder::getFirstUpgrade() != UpgradeTypes::None && Broodwar->self()->getUpgradeLevel(BuildOrder::getFirstUpgrade()) <= 0 && !Broodwar->self()->isUpgrading(BuildOrder::getFirstUpgrade()))
                    return false;
                if (BuildOrder::getFirstTech() != TechTypes::None && !Broodwar->self()->hasResearched(BuildOrder::getFirstTech()) && !Broodwar->self()->isResearching(BuildOrder::getFirstTech()))
                    return false;
            }

            if (Broodwar->self()->getRace() == Races::Protoss) {
                switch (tech) {
                case Psionic_Storm:
                    return true;
                case Stasis_Field:
                    return Broodwar->self()->getUpgradeLevel(UpgradeTypes::Khaydarin_Core) > 0;
                case Recall:
                    return (Broodwar->self()->hasResearched(TechTypes::Stasis_Field) && Broodwar->self()->minerals() > 1500 && Broodwar->self()->gas() > 1000);
                case Disruption_Web:
                    return (vis(Protoss_Corsair) >= 10);
                }
            }

            else if (Broodwar->self()->getRace() == Races::Terran) {
                switch (tech) {
                case Stim_Packs:
                    return true;
                case Spider_Mines:
                    return Broodwar->self()->getUpgradeLevel(UpgradeTypes::Ion_Thrusters) > 0 || Broodwar->self()->isUpgrading(UpgradeTypes::Ion_Thrusters);
                case Tank_Siege_Mode:
                    return Broodwar->self()->hasResearched(TechTypes::Spider_Mines) || Broodwar->self()->isResearching(TechTypes::Spider_Mines) || vis(Terran_Siege_Tank_Tank_Mode) > 0;
                case Cloaking_Field:
                    return vis(Terran_Wraith) >= 2;
                case Yamato_Gun:
                    return vis(Terran_Battlecruiser) >= 0;
                case Personnel_Cloaking:
                    return vis(Terran_Ghost) >= 2;
                }
            }

            else if (Broodwar->self()->getRace() == Races::Zerg) {
                switch (tech) {
                case Lurker_Aspect:
                    return !BuildOrder::isTechUnit(Zerg_Hydralisk) || (haveOrUpgrading(UpgradeTypes::Grooved_Spines, 1) && haveOrUpgrading(UpgradeTypes::Muscular_Augments, 1));
                case Burrowing:
                    return Stations::getStations(PlayerState::Self).size() >= 3 && Players::getSupply(PlayerState::Self, Races::Zerg) > 140;
                case Consume:
                    return true;
                case Plague:
                    return Broodwar->self()->hasResearched(Consume);
                }
            }
            return false;
        }

        bool addon(UnitInfo& building)
        {
            for (auto &unit : building.getType().buildsWhat()) {
                if (unit.isAddon() && BuildOrder::buildCount(unit) > vis(unit)) {
                    building.unit()->buildAddon(unit);
                    return true;
                }
            }
            return false;
        }

        bool produce(UnitInfo& building)
        {
            auto offset = 16;
            auto best = 0.0;
            auto bestType = None;

            // Clear idle checks
            auto idleItr = idleProduction.find(building.unit());
            if (idleItr != idleProduction.end()) {
                reservedMineral -= idleItr->second.mineralPrice();
                reservedGas -= idleItr->second.gasPrice();
                idleProduction.erase(building.unit());
            }

            // Choose an Overlord if we need one
            if (building.getType() == Zerg_Larva && BuildOrder::buildCount(Zerg_Overlord) > vis(Zerg_Overlord) + trainedThisFrame[Zerg_Overlord]) {
                if (isAffordable(Zerg_Overlord)) {
                    building.unit()->morph(Zerg_Overlord);
                    building.setRemainingTrainFrame(bestType.buildTime());
                    trainedThisFrame[Zerg_Overlord]++;
                    lastTrainFrame = Broodwar->getFrameCount();
                }
                return true;
            }

            // Look through each UnitType this can train
            for (auto &type : building.getType().buildsWhat()) {

                if (!isCreateable(building.unit(), type)
                    || !isSuitable(type))
                    continue;

                const auto value = scoreUnit(type);
                if (value > best) {
                    best = value;
                    bestType = type;
                }
            }

            if (bestType != None) {

                // If we can afford it, train it
                if (isAffordable(bestType)) {
                    trainedThisFrame[bestType]++;
                    building.unit()->train(bestType);
                    building.setRemainingTrainFrame(bestType.buildTime());
                    idleProduction.erase(building.unit());
                    lastTrainFrame = Broodwar->getFrameCount();
                    return true;
                }

                // Else if this is a tech unit, add it to idle production
                else if (BuildOrder::isTechUnit(bestType) && Broodwar->self()->getRace() != Races::Zerg && (Workers::getMineralWorkers() > 0 || Broodwar->self()->minerals() >= bestType.mineralPrice()) && (Workers::getGasWorkers() > 0 || Broodwar->self()->gas() >= bestType.gasPrice())) {
                    trainedThisFrame[bestType]++;
                    idleProduction[building.unit()] = bestType;
                    reservedMineral += bestType.mineralPrice();
                    reservedGas += bestType.gasPrice();
                }

                // Else store a zero value idle
                else
                    idleProduction[building.unit()] = None;
            }
            return false;
        }

        bool research(UnitInfo& building)
        {
            // Clear idle checks
            auto idleItr = idleTech.find(building.unit());
            if (idleItr != idleTech.end()) {
                reservedMineral -= idleItr->second.mineralPrice();
                reservedGas -= idleItr->second.gasPrice();
                idleTech.erase(building.unit());
            }

            for (auto &research : building.getType().researchesWhat()) {
                if (isCreateable(building.unit(), research) && isSuitable(research)) {
                    if (isAffordable(research)) {
                        building.setRemainingTrainFrame(research.researchTime());
                        building.unit()->research(research);
                        lastTrainFrame = Broodwar->getFrameCount();
                        return true;
                    }
                    else if ((Workers::getMineralWorkers() > 0 || Broodwar->self()->minerals() >= research.mineralPrice()) && (Workers::getGasWorkers() > 0 || Broodwar->self()->gas() >= research.gasPrice())) {
                        idleTech[building.unit()] = research;
                        reservedMineral += research.mineralPrice();
                        reservedGas += research.gasPrice();
                    }
                    break;
                }
            }
            return false;
        }

        bool upgrade(UnitInfo& building)
        {
            // Clear idle checks
            auto idleItr = idleUpgrade.find(building.unit());
            if (idleItr != idleUpgrade.end()) {
                reservedMineral -= idleItr->second.mineralPrice();
                reservedGas -= idleItr->second.gasPrice();
                idleUpgrade.erase(building.unit());
            }

            int offset = 0;
            for (auto &upgrade : building.getType().upgradesWhat()) {
                if (isCreateable(building.unit(), upgrade) && isSuitable(upgrade)) {
                    if (isAffordable(upgrade)) {
                        building.setRemainingTrainFrame(upgrade.upgradeTime(Broodwar->self()->getUpgradeLevel(upgrade) + 1));
                        building.unit()->upgrade(upgrade);
                        lastTrainFrame = Broodwar->getFrameCount();
                        return true;
                    }
                    else if ((Workers::getMineralWorkers() > 0 || Broodwar->self()->minerals() >= upgrade.mineralPrice()) && (Workers::getGasWorkers() > 0 || Broodwar->self()->gas() >= upgrade.gasPrice())) {
                        idleUpgrade[building.unit()] = upgrade;
                        reservedMineral += upgrade.mineralPrice();
                        reservedGas += upgrade.gasPrice();
                    }
                    break;
                }
            }
            return false;
        }

        void updateReservedResources()
        {
            // Reserved minerals for idle buildings, tech and upgrades
            for (auto &[unit, type] : idleProduction) {
                if (BuildOrder::isTechUnit(type) && unit->exists()) {
                    reservedMineral += type.mineralPrice();
                    reservedGas += type.gasPrice();
                }
            }

            for (auto &[unit, tech] : idleTech) {
                if (unit->exists()) {
                    reservedMineral += tech.mineralPrice();
                    reservedGas += tech.gasPrice();
                }
            }

            for (auto &[unit, upgrade] : idleUpgrade) {
                if (unit->exists()) {
                    reservedMineral += upgrade.mineralPrice();
                    reservedGas += upgrade.gasPrice();
                }
            }
        }

        void updateLarva()
        {
            for (int i = 0; i < vis(Zerg_Larva); i++) {

                // Find the best UnitType
                auto best = 0.0;
                auto bestType = None;

                // Choose an Overlord if we need one
                if (BuildOrder::buildCount(Zerg_Overlord) > vis(Zerg_Overlord) + trainedThisFrame[Zerg_Overlord])
                    bestType = Zerg_Overlord;
                else {
                    for (auto &type : Zerg_Larva.buildsWhat()) {
                        if (!isCreateable(nullptr, type)
                            || !isSuitable(type))
                            continue;

                        const auto value = scoreUnit(type);
                        if (value >= best) {
                            best = value;
                            bestType = type;
                        }
                    }
                }

                auto validLarva = [&](UnitInfo &larva, double saturation, BWEB::Station * station) {
                    if (!larva.unit()
                        || larva.getType() != Zerg_Larva
                        || larva.getRole() != Role::Production
                        || (bestType == Zerg_Overlord && !station->isNatural() && Players::ZvP() && Util::getTime() > Time(4, 00) && Util::getTime() < Time(7, 00) && Spy::getEnemyTransition() == "Corsair")
                        || ((bestType == Zerg_Overlord || bestType.isWorker()) && larva.isProxy())
                        || (larva.isProxy() && bestType != Zerg_Hydralisk && BuildOrder::getCurrentTransition().find("Lurker") != string::npos)
                        || (!larva.isProxy() && BuildOrder::isProxy() && bestType == Zerg_Hydralisk)
                        || !larva.unit()->getHatchery()
                        || !larva.unit()->isCompleted()
                        || larva.getRemainingTrainFrames() >= Broodwar->getLatencyFrames()
                        || lastTrainFrame >= Broodwar->getFrameCount() - Broodwar->getLatencyFrames() - 4
                        || (Planning::overlapsPlan(larva, larva.getPosition()) && Util::getTime() > Time(4, 00)))
                        return false;

                    auto closestStation = Stations::getClosestStationAir(larva.getPosition(), PlayerState::Self);
                    return station == closestStation;
                };

                // Find a station that we should morph at based on the UnitType we want.
                auto produced = false;
                multimap<double, BWEB::Station*> stations;
                for (auto &[val, station] : Stations::getStationsBySaturation()) {
                    auto saturation = bestType.isWorker() ? val : 1.0 / val;
                    auto larvaCount = count_if(Units::getUnits(PlayerState::Self).begin(), Units::getUnits(PlayerState::Self).end(), [&](auto &u) {
                        auto &unit = *u;
                        return unit.getType() == Zerg_Larva && unit.unit()->getHatchery() && unit.unit()->getHatchery()->getTilePosition() == station->getBase()->Location();
                    });
                    if (larvaCount >= 3 && bestType != Zerg_Drone)
                        saturation = 1.0 / 24.0;

                    stations.emplace(saturation * larvaCount, station);
                }

                for (auto &[val, station] : stations) {
                    for (auto &u : Units::getUnits(PlayerState::Self)) {
                        UnitInfo &larva = *u;
                        if (!larvaTrickRequired(larva)) {
                            if (validLarva(larva, val, station)) {
                                produce(larva);
                                produced = true;
                            }
                            else if (larvaTrickOptional(larva))
                                continue;
                            if (produced)
                                return;
                        }
                    }
                }
            }
        }

        void updateProduction()
        {
            const auto commands ={ addon, produce, research, upgrade };
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                UnitInfo &building = *u;

                if (!building.unit()
                    || building.getRole() != Role::Production
                    || !building.unit()->isCompleted()
                    || building.getRemainingTrainFrames() >= Broodwar->getLatencyFrames()
                    || lastTrainFrame >= Broodwar->getFrameCount() - Broodwar->getLatencyFrames() - 4
                    || building.getType() == Zerg_Larva)
                    continue;

                // Iterate commmands and break if we execute one
                for (auto &command : commands) {
                    if (command(building))
                        break;
                }
            }
        }
    }

    void onFrame()
    {
        Visuals::startPerfTest();
        reset();
        updateReservedResources();
        updateLarva();
        updateProduction();
        Visuals::endPerfTest("Production");
    }

    double scoreUnit(UnitType type)
    {
        if (BuildOrder::getTechUnit() == type && Broodwar->self()->getRace() != Races::Zerg)
            return DBL_MAX;

        // Check if we are saving larva but not for this type
        if (BuildOrder::getUnitReservation(type) == 0 && (BuildOrder::getUnitReservation(Zerg_Mutalisk) > 0 || BuildOrder::getUnitReservation(Zerg_Hydralisk) > 0)) {
            auto larvaMinCost = (BuildOrder::getTechUnit().mineralPrice() * BuildOrder::getUnitReservation(Zerg_Mutalisk))
                + (Zerg_Hydralisk.mineralPrice() * BuildOrder::getUnitReservation(Zerg_Hydralisk))
                + (Zerg_Scourge.mineralPrice() * BuildOrder::getUnitReservation(Zerg_Scourge));

            auto larvaGasCost = (Zerg_Mutalisk.gasPrice() * BuildOrder::getUnitReservation(Zerg_Mutalisk))
                + (Zerg_Hydralisk.gasPrice() * BuildOrder::getUnitReservation(Zerg_Hydralisk))
                + (Zerg_Scourge.gasPrice() * BuildOrder::getUnitReservation(Zerg_Scourge));

            auto larvaRequirements = BuildOrder::getUnitReservation(Zerg_Mutalisk) + BuildOrder::getUnitReservation(Zerg_Hydralisk) + (BuildOrder::getUnitReservation(Zerg_Scourge) / 2);

            if ((type != Zerg_Overlord && vis(Zerg_Larva) <= larvaRequirements)
                || Broodwar->self()->minerals() - type.mineralPrice() < larvaMinCost
                || Broodwar->self()->gas() - type.gasPrice() < larvaGasCost)
                return 0.0;
        }

        auto percentage = BuildOrder::getCompositionPercentage(type);
        auto trainedCount = vis(type) + trainedThisFrame[type];

        auto typeMineralCost = Math::realisticMineralCost(type);
        auto typeGasCost = Math::realisticGasCost(type);

        for (auto &idleType : idleProduction) {
            if (type == idleType.second)
                trainedCount++;
        }

        for (auto &secondType : type.buildsWhat()) {
            if (!secondType.isBuilding()) {
                trainedCount += vis(secondType) + trainedThisFrame[secondType];
                typeMineralCost += (int)round(BuildOrder::getCompositionPercentage(secondType) * BuildOrder::getCompositionPercentage(type) * Math::realisticMineralCost(secondType));
                typeGasCost += (int)round(BuildOrder::getCompositionPercentage(secondType) * BuildOrder::getCompositionPercentage(type) * Math::realisticGasCost(secondType));
            }
        }

        auto mineralCost = (Broodwar->self()->minerals() == 0 || typeMineralCost == 0) ? 1.0 : max(1.0, double((Broodwar->self()->minerals() * 2) - typeMineralCost - reservedMineral)) / double(Broodwar->self()->minerals());
        auto gasCost = (Broodwar->self()->gas() == 0 || typeGasCost == 0) ? 1.0 : max(1.0, double((Broodwar->self()->gas() * 2) - typeGasCost - reservedGas)) / double(Broodwar->self()->gas());

        // Can't make them if we aren't mining and can't afford
        if ((Workers::getGasWorkers() == 0 && typeGasCost > 0 && Broodwar->self()->gas() < typeGasCost)
            || (Workers::getMineralWorkers() == 0 && typeMineralCost > 0 && Broodwar->self()->minerals() < typeMineralCost))
            return 0.0;

        const auto resourceScore = gasCost * mineralCost;
        const auto strategyScore = 100.0 * percentage / double(max(1, trainedCount));
        return resourceScore + strategyScore;
    }

    bool larvaTrickRequired(UnitInfo& larva) {
        if (larva.getType() == Zerg_Larva && larva.unit()->getHatchery()) {
            auto closestStation = Stations::getClosestStationAir(larva.unit()->getHatchery()->getPosition(), PlayerState::Self);
            if (!closestStation)
                return false;

            auto mustMoveToLeft = Planning::overlapsPlan(larva, larva.getPosition());
            auto canMove = (larva.getPosition().y - 16.0 > larva.unit()->getHatchery()->getPosition().y || larva.getPosition().x + 24 > larva.unit()->getHatchery()->getPosition().x);
            if (canMove && mustMoveToLeft) {
                if (larva.unit()->getLastCommand().getType() != UnitCommandTypes::Stop)
                    larva.unit()->stop();
                return true;
            }
        }
        return false;
    };

    bool larvaTrickOptional(UnitInfo& larva) {
        if (larva.getType() == Zerg_Larva && larva.unit()->getHatchery()) {
            auto closestStation = Stations::getClosestStationAir(larva.unit()->getHatchery()->getPosition(), PlayerState::Self);
            if (!closestStation)
                return false;

            auto tryMoveToLeft = closestStation->getResourceCentroid().y + 64.0 < closestStation->getBase()->Center().y || closestStation->getResourceCentroid().x < closestStation->getBase()->Center().x;
            auto canMove = (larva.getPosition().y - 16.0 > larva.unit()->getHatchery()->getPosition().y || larva.getPosition().x + 24 > larva.unit()->getHatchery()->getPosition().x);
            if (canMove && tryMoveToLeft) {
                if (larva.unit()->getLastCommand().getType() != UnitCommandTypes::Stop)
                    larva.unit()->stop();
                return true;
            }
        }
        return false;
    };

    int getReservedMineral() { return reservedMineral; }
    int getReservedGas() { return reservedGas; }
    bool hasIdleProduction() { return int(idleProduction.size()) > 0; }
}