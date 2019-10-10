#include <string>
#include <map>
#include <set>
#include <BWAPI/UpgradeType.h>
#include <BWAPI/Race.h>
#include <BWAPI/UnitType.h>
#include <Util/Foreach.h>

#include "Common.h"

namespace BWAPI
{
  bool initializingUpgradeType = true;
  class UpgradeTypeInternal
  {
    public:
      UpgradeTypeInternal() {valid = false;}
      void set(const char* name, int mineralPriceBase, int mineralPriceFactor, int gasPriceBase, int gasPriceFactor, int upgradeTimeBase, int upgradeTimeFactor, BWAPI::UnitType whatUpgrades, Race race, BWAPI::UnitType whatUses, int maxRepeats)
      {
        if (initializingUpgradeType)
        {
          this->name               = name;
          this->mineralPriceBase   = mineralPriceBase;
          this->mineralPriceFactor = mineralPriceFactor;
          this->gasPriceBase       = gasPriceBase;
          this->gasPriceFactor     = gasPriceFactor;
          this->upgradeTimeBase    = upgradeTimeBase;
          this->upgradeTimeFactor  = upgradeTimeFactor;
          this->whatUpgrades       = whatUpgrades;
          this->race               = race;
          if (whatUses != UnitTypes::None)
            this->whatUses.insert(whatUses);

          this->maxRepeats = maxRepeats;
          this->valid      = true;
        }
      }
      std::string name;
      int mineralPriceBase;
      int mineralPriceFactor;
      int gasPriceBase;
      int gasPriceFactor;
      int upgradeTimeBase;
      int upgradeTimeFactor;
      BWAPI::UnitType whatUpgrades;
      Race race;
      int maxRepeats;
      std::set<BWAPI::UnitType> whatUses;
      bool valid;
  };
  UpgradeTypeInternal upgradeTypeData[63];
  std::map<std::string, UpgradeType> upgradeTypeMap;
  std::set< UpgradeType > upgradeTypeSet;
  namespace UpgradeTypes
  {
    const UpgradeType Terran_Infantry_Armor(0);
    const UpgradeType Terran_Vehicle_Plating(1);
    const UpgradeType Terran_Ship_Plating(2);
    const UpgradeType Zerg_Carapace(3);
    const UpgradeType Zerg_Flyer_Carapace(4);
    const UpgradeType Protoss_Ground_Armor(5);
    const UpgradeType Protoss_Air_Armor(6);
    const UpgradeType Terran_Infantry_Weapons(7);
    const UpgradeType Terran_Vehicle_Weapons(8);
    const UpgradeType Terran_Ship_Weapons(9);
    const UpgradeType Zerg_Melee_Attacks(10);
    const UpgradeType Zerg_Missile_Attacks(11);
    const UpgradeType Zerg_Flyer_Attacks(12);
    const UpgradeType Protoss_Ground_Weapons(13);
    const UpgradeType Protoss_Air_Weapons(14);
    const UpgradeType Protoss_Plasma_Shields(15);
    const UpgradeType U_238_Shells(16);
    const UpgradeType Ion_Thrusters(17);
    const UpgradeType Titan_Reactor(19);
    const UpgradeType Ocular_Implants(20);
    const UpgradeType Moebius_Reactor(21);
    const UpgradeType Apollo_Reactor(22);
    const UpgradeType Colossus_Reactor(23);
    const UpgradeType Ventral_Sacs(24);
    const UpgradeType Antennae(25);
    const UpgradeType Pneumatized_Carapace(26);
    const UpgradeType Metabolic_Boost(27);
    const UpgradeType Adrenal_Glands(28);
    const UpgradeType Muscular_Augments(29);
    const UpgradeType Grooved_Spines(30);
    const UpgradeType Gamete_Meiosis(31);
    const UpgradeType Metasynaptic_Node(32);
    const UpgradeType Singularity_Charge(33);
    const UpgradeType Leg_Enhancements(34);
    const UpgradeType Scarab_Damage(35);
    const UpgradeType Reaver_Capacity(36);
    const UpgradeType Gravitic_Drive(37);
    const UpgradeType Sensor_Array(38);
    const UpgradeType Gravitic_Boosters(39);
    const UpgradeType Khaydarin_Amulet(40);
    const UpgradeType Apial_Sensors(41);
    const UpgradeType Gravitic_Thrusters(42);
    const UpgradeType Carrier_Capacity(43);
    const UpgradeType Khaydarin_Core(44);
    const UpgradeType Argus_Jewel(47);
    const UpgradeType Argus_Talisman(49);
    const UpgradeType Caduceus_Reactor(51);
    const UpgradeType Chitinous_Plating(52);
    const UpgradeType Anabolic_Synthesis(53);
    const UpgradeType Charon_Boosters(54);
    const UpgradeType None(61);
    const UpgradeType Unknown(62);

    void init()
    {
      upgradeTypeData[Terran_Infantry_Armor.getID()].set("Terran Infantry Armor"    , 100, 75 , 100, 75 , 4000, 480, UnitTypes::Terran_Engineering_Bay      , Races::Terran , UnitTypes::None                 , 3);
      upgradeTypeData[Terran_Vehicle_Plating.getID()].set("Terran Vehicle Plating"  , 100, 75 , 100, 75 , 4000, 480, UnitTypes::Terran_Armory               , Races::Terran , UnitTypes::None                 , 3);
      upgradeTypeData[Terran_Ship_Plating.getID()].set("Terran Ship Plating"        , 150, 75 , 150, 75 , 4000, 480, UnitTypes::Terran_Armory               , Races::Terran , UnitTypes::None                 , 3);
      upgradeTypeData[Zerg_Carapace.getID()].set("Zerg Carapace"                    , 150, 75 , 150, 75 , 4000, 480, UnitTypes::Zerg_Evolution_Chamber      , Races::Zerg   , UnitTypes::None                 , 3);
      upgradeTypeData[Zerg_Flyer_Carapace.getID()].set("Zerg Flyer Carapace"        , 150, 75 , 150, 75 , 4000, 480, UnitTypes::Zerg_Spire                  , Races::Zerg   , UnitTypes::None                 , 3);
      upgradeTypeData[Protoss_Ground_Armor.getID()].set("Protoss Ground Armor"      , 100, 75 , 100, 75 , 4000, 480, UnitTypes::Protoss_Forge               , Races::Protoss, UnitTypes::None                 , 3);
      upgradeTypeData[Protoss_Air_Armor.getID()].set("Protoss Air Armor"            , 150, 75 , 150, 75 , 4000, 480, UnitTypes::Protoss_Cybernetics_Core    , Races::Protoss, UnitTypes::None                 , 3);
      upgradeTypeData[Terran_Infantry_Weapons.getID()].set("Terran Infantry Weapons", 100, 75 , 100, 75 , 4000, 480, UnitTypes::Terran_Engineering_Bay      , Races::Terran , UnitTypes::None                 , 3);
      upgradeTypeData[Terran_Vehicle_Weapons.getID()].set("Terran Vehicle Weapons"  , 100, 75 , 100, 75 , 4000, 480, UnitTypes::Terran_Armory               , Races::Terran , UnitTypes::None                 , 3);
      upgradeTypeData[Terran_Ship_Weapons.getID()].set("Terran Ship Weapons"        , 100, 50 , 100, 50 , 4000, 480, UnitTypes::Terran_Armory               , Races::Terran , UnitTypes::None                 , 3);
      upgradeTypeData[Zerg_Melee_Attacks.getID()].set("Zerg Melee Attacks"          , 100, 50 , 100, 50 , 4000, 480, UnitTypes::Zerg_Evolution_Chamber      , Races::Zerg   , UnitTypes::None                 , 3);
      upgradeTypeData[Zerg_Missile_Attacks.getID()].set("Zerg Missile Attacks"      , 100, 50 , 100, 50 , 4000, 480, UnitTypes::Zerg_Evolution_Chamber      , Races::Zerg   , UnitTypes::None                 , 3);
      upgradeTypeData[Zerg_Flyer_Attacks.getID()].set("Zerg Flyer Attacks"          , 100, 75 , 100, 75 , 4000, 480, UnitTypes::Zerg_Spire                  , Races::Zerg   , UnitTypes::None                 , 3);
      upgradeTypeData[Protoss_Ground_Weapons.getID()].set("Protoss Ground Weapons"  , 100, 50 , 100, 50 , 4000, 480, UnitTypes::Protoss_Forge               , Races::Protoss, UnitTypes::None                 , 3);
      upgradeTypeData[Protoss_Air_Weapons.getID()].set("Protoss Air Weapons"        , 100, 75 , 100, 75 , 4000, 480, UnitTypes::Protoss_Cybernetics_Core    , Races::Protoss, UnitTypes::None                 , 3);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].set("Protoss Plasma Shields"  , 200, 100, 200, 100, 4000, 480, UnitTypes::Protoss_Forge               , Races::Protoss, UnitTypes::None                 , 3);
      upgradeTypeData[U_238_Shells.getID()].set("U-238 Shells"                      , 150, 0  , 150, 0  , 1500, 0  , UnitTypes::Terran_Academy              , Races::Terran , UnitTypes::Terran_Marine        , 1);
      upgradeTypeData[Ion_Thrusters.getID()].set("Ion Thrusters"                    , 100, 0  , 100, 0  , 1500, 0  , UnitTypes::Terran_Machine_Shop         , Races::Terran , UnitTypes::Terran_Vulture       , 1);
      upgradeTypeData[Titan_Reactor.getID()].set("Titan Reactor"                    , 150, 0  , 150, 0  , 2500, 0  , UnitTypes::Terran_Science_Facility     , Races::Terran , UnitTypes::Terran_Science_Vessel, 1);
      upgradeTypeData[Ocular_Implants.getID()].set("Ocular Implants"                , 100, 0  , 100, 0  , 2500, 0  , UnitTypes::Terran_Covert_Ops           , Races::Terran , UnitTypes::Terran_Ghost         , 1);
      upgradeTypeData[Moebius_Reactor.getID()].set("Moebius Reactor"                , 150, 0  , 150, 0  , 2500, 0  , UnitTypes::Terran_Covert_Ops           , Races::Terran , UnitTypes::Terran_Ghost         , 1);
      upgradeTypeData[Apollo_Reactor.getID()].set("Apollo Reactor"                  , 200, 0  , 200, 0  , 2500, 0  , UnitTypes::Terran_Control_Tower        , Races::Terran , UnitTypes::Terran_Wraith        , 1);
      upgradeTypeData[Colossus_Reactor.getID()].set("Colossus Reactor"              , 150, 0  , 150, 0  , 2500, 0  , UnitTypes::Terran_Physics_Lab          , Races::Terran , UnitTypes::Terran_Battlecruiser , 1);
      upgradeTypeData[Ventral_Sacs.getID()].set("Ventral Sacs"                      , 200, 0  , 200, 0  , 2400, 0  , UnitTypes::Zerg_Lair                   , Races::Zerg   , UnitTypes::Zerg_Overlord        , 1);
      upgradeTypeData[Antennae.getID()].set("Antennae"                              , 150, 0  , 150, 0  , 2000, 0  , UnitTypes::Zerg_Lair                   , Races::Zerg   , UnitTypes::Zerg_Overlord        , 1);
      upgradeTypeData[Pneumatized_Carapace.getID()].set("Pneumatized Carapace"      , 150, 0  , 150, 0  , 2000, 0  , UnitTypes::Zerg_Lair                   , Races::Zerg   , UnitTypes::Zerg_Overlord        , 1);
      upgradeTypeData[Metabolic_Boost.getID()].set("Metabolic Boost"                , 100, 0  , 100, 0  , 1500, 0  , UnitTypes::Zerg_Spawning_Pool          , Races::Zerg   , UnitTypes::Zerg_Zergling        , 1);
      upgradeTypeData[Adrenal_Glands.getID()].set("Adrenal Glands"                  , 200, 0  , 200, 0  , 1500, 0  , UnitTypes::Zerg_Spawning_Pool          , Races::Zerg   , UnitTypes::Zerg_Zergling        , 1);
      upgradeTypeData[Muscular_Augments.getID()].set("Muscular Augments"            , 150, 0  , 150, 0  , 1500, 0  , UnitTypes::Zerg_Hydralisk_Den          , Races::Zerg   , UnitTypes::Zerg_Hydralisk       , 1);
      upgradeTypeData[Grooved_Spines.getID()].set("Grooved Spines"                  , 150, 0  , 150, 0  , 1500, 0  , UnitTypes::Zerg_Hydralisk_Den          , Races::Zerg   , UnitTypes::Zerg_Hydralisk       , 1);
      upgradeTypeData[Gamete_Meiosis.getID()].set("Gamete Meiosis"                  , 150, 0  , 150, 0  , 2500, 0  , UnitTypes::Zerg_Queens_Nest            , Races::Zerg   , UnitTypes::Zerg_Queen           , 1);
      upgradeTypeData[Metasynaptic_Node.getID()].set("Metasynaptic Node"            , 150, 0  , 150, 0  , 2500, 0  , UnitTypes::Zerg_Defiler_Mound          , Races::Zerg   , UnitTypes::Zerg_Defiler         , 1);
      upgradeTypeData[Singularity_Charge.getID()].set("Singularity Charge"          , 150, 0  , 150, 0  , 2500, 0  , UnitTypes::Protoss_Cybernetics_Core    , Races::Protoss, UnitTypes::Protoss_Dragoon      , 1);
      upgradeTypeData[Leg_Enhancements.getID()].set("Leg Enhancements"              , 150, 0  , 150, 0  , 2000, 0  , UnitTypes::Protoss_Citadel_of_Adun     , Races::Protoss, UnitTypes::Protoss_Zealot       , 1);
      upgradeTypeData[Scarab_Damage.getID()].set("Scarab Damage"                    , 200, 0  , 200, 0  , 2500, 0  , UnitTypes::Protoss_Robotics_Support_Bay, Races::Protoss, UnitTypes::Protoss_Scarab       , 1);
      upgradeTypeData[Reaver_Capacity.getID()].set("Reaver Capacity"                , 200, 0  , 200, 0  , 2500, 0  , UnitTypes::Protoss_Robotics_Support_Bay, Races::Protoss, UnitTypes::Protoss_Reaver       , 1);
      upgradeTypeData[Gravitic_Drive.getID()].set("Gravitic Drive"                  , 200, 0  , 200, 0  , 2500, 0  , UnitTypes::Protoss_Robotics_Support_Bay, Races::Protoss, UnitTypes::Protoss_Shuttle      , 1);
      upgradeTypeData[Sensor_Array.getID()].set("Sensor Array"                      , 150, 0  , 150, 0  , 2000, 0  , UnitTypes::Protoss_Observatory         , Races::Protoss, UnitTypes::Protoss_Observer     , 1);
      upgradeTypeData[Gravitic_Boosters.getID()].set("Gravitic Boosters"            , 150, 0  , 150, 0  , 2000, 0  , UnitTypes::Protoss_Observatory         , Races::Protoss, UnitTypes::Protoss_Observer     , 1);
      upgradeTypeData[Khaydarin_Amulet.getID()].set("Khaydarin Amulet"              , 150, 0  , 150, 0  , 2500, 0  , UnitTypes::Protoss_Templar_Archives    , Races::Protoss, UnitTypes::Protoss_High_Templar , 1);
      upgradeTypeData[Apial_Sensors.getID()].set("Apial Sensors"                    , 100, 0  , 100, 0  , 2500, 0  , UnitTypes::Protoss_Fleet_Beacon        , Races::Protoss, UnitTypes::Protoss_Scout        , 1);
      upgradeTypeData[Gravitic_Thrusters.getID()].set("Gravitic Thrusters"          , 200, 0  , 200, 0  , 2500, 0  , UnitTypes::Protoss_Fleet_Beacon        , Races::Protoss, UnitTypes::Protoss_Scout        , 1);
      upgradeTypeData[Carrier_Capacity.getID()].set("Carrier Capacity"              , 100, 0  , 100, 0  , 1500, 0  , UnitTypes::Protoss_Fleet_Beacon        , Races::Protoss, UnitTypes::Protoss_Carrier      , 1);
      upgradeTypeData[Khaydarin_Core.getID()].set("Khaydarin Core"                  , 150, 0  , 150, 0  , 2500, 0  , UnitTypes::Protoss_Arbiter_Tribunal    , Races::Protoss, UnitTypes::Protoss_Arbiter      , 1);
      upgradeTypeData[Argus_Jewel.getID()].set("Argus Jewel"                        , 100, 0  , 100, 0  , 2500, 0  , UnitTypes::Protoss_Fleet_Beacon        , Races::Protoss, UnitTypes::Protoss_Corsair      , 1);
      upgradeTypeData[Argus_Talisman.getID()].set("Argus Talisman"                  , 150, 0  , 150, 0  , 2500, 0  , UnitTypes::Protoss_Templar_Archives    , Races::Protoss, UnitTypes::Protoss_Dark_Archon  , 1);
      upgradeTypeData[Caduceus_Reactor.getID()].set("Caduceus Reactor"              , 150, 0  , 150, 0  , 2500, 0  , UnitTypes::Terran_Academy              , Races::Terran , UnitTypes::Terran_Medic         , 1);
      upgradeTypeData[Chitinous_Plating.getID()].set("Chitinous Plating"            , 150, 0  , 150, 0  , 2000, 0  , UnitTypes::Zerg_Ultralisk_Cavern       , Races::Zerg   , UnitTypes::Zerg_Ultralisk       , 1);
      upgradeTypeData[Anabolic_Synthesis.getID()].set("Anabolic Synthesis"          , 200, 0  , 200, 0  , 2000, 0  , UnitTypes::Zerg_Ultralisk_Cavern       , Races::Zerg   , UnitTypes::Zerg_Ultralisk       , 1);
      upgradeTypeData[Charon_Boosters.getID()].set("Charon Boosters"                , 100, 0  , 100, 0  , 2000, 0  , UnitTypes::Terran_Machine_Shop         , Races::Terran , UnitTypes::Terran_Goliath       , 1);
      upgradeTypeData[None.getID()].set("None"                                      , 0  , 0  , 0  , 0  , 0   , 0  , UnitTypes::None                        , Races::None   , UnitTypes::None                 , 0);
      upgradeTypeData[Unknown.getID()].set("Unknown"                                , 0  , 0  , 0  , 0  , 0   , 0  , UnitTypes::None                        , Races::Unknown, UnitTypes::None                 , 0);

      upgradeTypeData[Terran_Infantry_Armor.getID()].whatUses.insert(UnitTypes::Terran_Firebat);
      upgradeTypeData[Terran_Infantry_Armor.getID()].whatUses.insert(UnitTypes::Terran_Ghost);
      upgradeTypeData[Terran_Infantry_Armor.getID()].whatUses.insert(UnitTypes::Terran_Marine);
      upgradeTypeData[Terran_Infantry_Armor.getID()].whatUses.insert(UnitTypes::Terran_Medic);
      upgradeTypeData[Terran_Infantry_Armor.getID()].whatUses.insert(UnitTypes::Terran_SCV);
      upgradeTypeData[Terran_Vehicle_Plating.getID()].whatUses.insert(UnitTypes::Terran_Goliath);
      upgradeTypeData[Terran_Vehicle_Plating.getID()].whatUses.insert(UnitTypes::Terran_Siege_Tank_Siege_Mode);
      upgradeTypeData[Terran_Vehicle_Plating.getID()].whatUses.insert(UnitTypes::Terran_Siege_Tank_Tank_Mode);
      upgradeTypeData[Terran_Vehicle_Plating.getID()].whatUses.insert(UnitTypes::Terran_Vulture);
      upgradeTypeData[Terran_Ship_Plating.getID()].whatUses.insert(UnitTypes::Terran_Battlecruiser);
      upgradeTypeData[Terran_Ship_Plating.getID()].whatUses.insert(UnitTypes::Terran_Dropship);
      upgradeTypeData[Terran_Ship_Plating.getID()].whatUses.insert(UnitTypes::Terran_Science_Vessel);
      upgradeTypeData[Terran_Ship_Plating.getID()].whatUses.insert(UnitTypes::Terran_Valkyrie);
      upgradeTypeData[Terran_Ship_Plating.getID()].whatUses.insert(UnitTypes::Terran_Wraith);
      upgradeTypeData[Zerg_Carapace.getID()].whatUses.insert(UnitTypes::Zerg_Broodling);
      upgradeTypeData[Zerg_Carapace.getID()].whatUses.insert(UnitTypes::Zerg_Defiler);
      upgradeTypeData[Zerg_Carapace.getID()].whatUses.insert(UnitTypes::Zerg_Drone);
      upgradeTypeData[Zerg_Carapace.getID()].whatUses.insert(UnitTypes::Zerg_Hydralisk);
      upgradeTypeData[Zerg_Carapace.getID()].whatUses.insert(UnitTypes::Zerg_Infested_Terran);
      upgradeTypeData[Zerg_Carapace.getID()].whatUses.insert(UnitTypes::Zerg_Larva);
      upgradeTypeData[Zerg_Carapace.getID()].whatUses.insert(UnitTypes::Zerg_Lurker);
      upgradeTypeData[Zerg_Carapace.getID()].whatUses.insert(UnitTypes::Zerg_Ultralisk);
      upgradeTypeData[Zerg_Carapace.getID()].whatUses.insert(UnitTypes::Zerg_Zergling);
      upgradeTypeData[Zerg_Flyer_Carapace.getID()].whatUses.insert(UnitTypes::Zerg_Devourer);
      upgradeTypeData[Zerg_Flyer_Carapace.getID()].whatUses.insert(UnitTypes::Zerg_Guardian);
      upgradeTypeData[Zerg_Flyer_Carapace.getID()].whatUses.insert(UnitTypes::Zerg_Mutalisk);
      upgradeTypeData[Zerg_Flyer_Carapace.getID()].whatUses.insert(UnitTypes::Zerg_Overlord);
      upgradeTypeData[Zerg_Flyer_Carapace.getID()].whatUses.insert(UnitTypes::Zerg_Queen);
      upgradeTypeData[Zerg_Flyer_Carapace.getID()].whatUses.insert(UnitTypes::Zerg_Scourge);
      upgradeTypeData[Protoss_Ground_Armor.getID()].whatUses.insert(UnitTypes::Protoss_Archon);
      upgradeTypeData[Protoss_Ground_Armor.getID()].whatUses.insert(UnitTypes::Protoss_Dark_Archon);
      upgradeTypeData[Protoss_Ground_Armor.getID()].whatUses.insert(UnitTypes::Protoss_Dark_Templar);
      upgradeTypeData[Protoss_Ground_Armor.getID()].whatUses.insert(UnitTypes::Protoss_Dragoon);
      upgradeTypeData[Protoss_Ground_Armor.getID()].whatUses.insert(UnitTypes::Protoss_High_Templar);
      upgradeTypeData[Protoss_Ground_Armor.getID()].whatUses.insert(UnitTypes::Protoss_Probe);
      upgradeTypeData[Protoss_Ground_Armor.getID()].whatUses.insert(UnitTypes::Protoss_Reaver);
      upgradeTypeData[Protoss_Ground_Armor.getID()].whatUses.insert(UnitTypes::Protoss_Zealot);
      upgradeTypeData[Protoss_Air_Armor.getID()].whatUses.insert(UnitTypes::Protoss_Arbiter);
      upgradeTypeData[Protoss_Air_Armor.getID()].whatUses.insert(UnitTypes::Protoss_Carrier);
      upgradeTypeData[Protoss_Air_Armor.getID()].whatUses.insert(UnitTypes::Protoss_Corsair);
      upgradeTypeData[Protoss_Air_Armor.getID()].whatUses.insert(UnitTypes::Protoss_Interceptor);
      upgradeTypeData[Protoss_Air_Armor.getID()].whatUses.insert(UnitTypes::Protoss_Observer);
      upgradeTypeData[Protoss_Air_Armor.getID()].whatUses.insert(UnitTypes::Protoss_Scout);
      upgradeTypeData[Protoss_Air_Armor.getID()].whatUses.insert(UnitTypes::Protoss_Shuttle);
      upgradeTypeData[Terran_Infantry_Weapons.getID()].whatUses.insert(UnitTypes::Terran_Firebat);
      upgradeTypeData[Terran_Infantry_Weapons.getID()].whatUses.insert(UnitTypes::Terran_Ghost);
      upgradeTypeData[Terran_Infantry_Weapons.getID()].whatUses.insert(UnitTypes::Terran_Marine);
      upgradeTypeData[Terran_Vehicle_Weapons.getID()].whatUses.insert(UnitTypes::Terran_Goliath);
      upgradeTypeData[Terran_Vehicle_Weapons.getID()].whatUses.insert(UnitTypes::Terran_Siege_Tank_Siege_Mode);
      upgradeTypeData[Terran_Vehicle_Weapons.getID()].whatUses.insert(UnitTypes::Terran_Siege_Tank_Tank_Mode);
      upgradeTypeData[Terran_Vehicle_Weapons.getID()].whatUses.insert(UnitTypes::Terran_Vulture);
      upgradeTypeData[Terran_Ship_Weapons.getID()].whatUses.insert(UnitTypes::Terran_Battlecruiser);
      upgradeTypeData[Terran_Ship_Weapons.getID()].whatUses.insert(UnitTypes::Terran_Valkyrie);
      upgradeTypeData[Terran_Ship_Weapons.getID()].whatUses.insert(UnitTypes::Terran_Wraith);
      upgradeTypeData[Zerg_Melee_Attacks.getID()].whatUses.insert(UnitTypes::Zerg_Broodling);
      upgradeTypeData[Zerg_Melee_Attacks.getID()].whatUses.insert(UnitTypes::Zerg_Ultralisk);
      upgradeTypeData[Zerg_Melee_Attacks.getID()].whatUses.insert(UnitTypes::Zerg_Zergling);
      upgradeTypeData[Zerg_Missile_Attacks.getID()].whatUses.insert(UnitTypes::Zerg_Hydralisk);
      upgradeTypeData[Zerg_Missile_Attacks.getID()].whatUses.insert(UnitTypes::Zerg_Lurker);
      upgradeTypeData[Zerg_Flyer_Attacks.getID()].whatUses.insert(UnitTypes::Zerg_Devourer);
      upgradeTypeData[Zerg_Flyer_Attacks.getID()].whatUses.insert(UnitTypes::Zerg_Guardian);
      upgradeTypeData[Zerg_Flyer_Attacks.getID()].whatUses.insert(UnitTypes::Zerg_Mutalisk);
      upgradeTypeData[Protoss_Ground_Weapons.getID()].whatUses.insert(UnitTypes::Protoss_Dark_Templar);
      upgradeTypeData[Protoss_Ground_Weapons.getID()].whatUses.insert(UnitTypes::Protoss_Dragoon);
      upgradeTypeData[Protoss_Ground_Weapons.getID()].whatUses.insert(UnitTypes::Protoss_Zealot);
      upgradeTypeData[Protoss_Air_Weapons.getID()].whatUses.insert(UnitTypes::Protoss_Arbiter);
      upgradeTypeData[Protoss_Air_Weapons.getID()].whatUses.insert(UnitTypes::Protoss_Corsair);
      upgradeTypeData[Protoss_Air_Weapons.getID()].whatUses.insert(UnitTypes::Protoss_Interceptor);
      upgradeTypeData[Protoss_Air_Weapons.getID()].whatUses.insert(UnitTypes::Protoss_Scout);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Arbiter);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Arbiter_Tribunal);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Archon);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Assimilator);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Carrier);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Citadel_of_Adun);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Corsair);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Cybernetics_Core);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Dark_Archon);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Dark_Templar);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Dragoon);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Fleet_Beacon);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Forge);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Gateway);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_High_Templar);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Interceptor);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Nexus);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Observatory);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Observer);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Photon_Cannon);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Probe);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Pylon);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Reaver);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Robotics_Facility);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Robotics_Support_Bay);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Scarab);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Scout);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Shield_Battery);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Shuttle);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Stargate);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Templar_Archives);
      upgradeTypeData[Protoss_Plasma_Shields.getID()].whatUses.insert(UnitTypes::Protoss_Zealot);

      upgradeTypeSet.insert(Terran_Infantry_Armor);
      upgradeTypeSet.insert(Terran_Vehicle_Plating);
      upgradeTypeSet.insert(Terran_Ship_Plating);
      upgradeTypeSet.insert(Zerg_Carapace);
      upgradeTypeSet.insert(Zerg_Flyer_Carapace);
      upgradeTypeSet.insert(Protoss_Ground_Armor);
      upgradeTypeSet.insert(Protoss_Air_Armor);
      upgradeTypeSet.insert(Terran_Infantry_Weapons);
      upgradeTypeSet.insert(Terran_Vehicle_Weapons);
      upgradeTypeSet.insert(Terran_Ship_Weapons);
      upgradeTypeSet.insert(Zerg_Melee_Attacks);
      upgradeTypeSet.insert(Zerg_Missile_Attacks);
      upgradeTypeSet.insert(Zerg_Flyer_Attacks);
      upgradeTypeSet.insert(Protoss_Ground_Weapons);
      upgradeTypeSet.insert(Protoss_Air_Weapons);
      upgradeTypeSet.insert(Protoss_Plasma_Shields);
      upgradeTypeSet.insert(U_238_Shells);
      upgradeTypeSet.insert(Ion_Thrusters);
      upgradeTypeSet.insert(Titan_Reactor);
      upgradeTypeSet.insert(Ocular_Implants);
      upgradeTypeSet.insert(Moebius_Reactor);
      upgradeTypeSet.insert(Apollo_Reactor);
      upgradeTypeSet.insert(Colossus_Reactor);
      upgradeTypeSet.insert(Ventral_Sacs);
      upgradeTypeSet.insert(Antennae);
      upgradeTypeSet.insert(Pneumatized_Carapace);
      upgradeTypeSet.insert(Metabolic_Boost);
      upgradeTypeSet.insert(Adrenal_Glands);
      upgradeTypeSet.insert(Muscular_Augments);
      upgradeTypeSet.insert(Grooved_Spines);
      upgradeTypeSet.insert(Gamete_Meiosis);
      upgradeTypeSet.insert(Metasynaptic_Node);
      upgradeTypeSet.insert(Singularity_Charge);
      upgradeTypeSet.insert(Leg_Enhancements);
      upgradeTypeSet.insert(Scarab_Damage);
      upgradeTypeSet.insert(Reaver_Capacity);
      upgradeTypeSet.insert(Gravitic_Drive);
      upgradeTypeSet.insert(Sensor_Array);
      upgradeTypeSet.insert(Gravitic_Boosters);
      upgradeTypeSet.insert(Khaydarin_Amulet);
      upgradeTypeSet.insert(Apial_Sensors);
      upgradeTypeSet.insert(Gravitic_Thrusters);
      upgradeTypeSet.insert(Carrier_Capacity);
      upgradeTypeSet.insert(Khaydarin_Core);
      upgradeTypeSet.insert(Argus_Jewel);
      upgradeTypeSet.insert(Argus_Talisman);
      upgradeTypeSet.insert(Caduceus_Reactor);
      upgradeTypeSet.insert(Chitinous_Plating);
      upgradeTypeSet.insert(Anabolic_Synthesis);
      upgradeTypeSet.insert(Charon_Boosters);
      upgradeTypeSet.insert(None);
      upgradeTypeSet.insert(Unknown);

      foreach(UpgradeType i, upgradeTypeSet)
      {
        std::string name = i.getName();
        fixName(&name);
        upgradeTypeMap.insert(std::make_pair(name, i));
      }
      initializingUpgradeType = false;
    }
  }
  UpgradeType::UpgradeType()
  {
    this->id = UpgradeTypes::None.id;
  }
  UpgradeType::UpgradeType(int id)
  {
    this->id = id;
    if (!initializingUpgradeType && (id < 0 || id >= 63 || !upgradeTypeData[id].valid) )
      this->id = UpgradeTypes::Unknown.id;
  }
  UpgradeType::UpgradeType(const UpgradeType& other)
  {
    this->id = other.id;
  }
  UpgradeType& UpgradeType::operator=(const UpgradeType& other)
  {
    this->id = other.id;
    return *this;
  }
  bool UpgradeType::operator==(const UpgradeType& other) const
  {
    return this->id == other.id;
  }
  bool UpgradeType::operator!=(const UpgradeType& other) const
  {
    return this->id != other.id;
  }
  bool UpgradeType::operator<(const UpgradeType& other) const
  {
    return this->id < other.id;
  }
  int UpgradeType::getID() const
  {
    return this->id;
  }
  std::string UpgradeType::getName() const
  {
    return upgradeTypeData[this->id].name;
  }
  Race UpgradeType::getRace() const
  {
    return upgradeTypeData[this->id].race;
  }
  int UpgradeType::mineralPrice() const
  {
    return upgradeTypeData[this->id].mineralPriceBase;
  }
  int UpgradeType::mineralPriceFactor() const
  {
    return upgradeTypeData[this->id].mineralPriceFactor;
  }
  int UpgradeType::gasPrice() const
  {
    return upgradeTypeData[this->id].gasPriceBase;
  }
  int UpgradeType::gasPriceFactor() const
  {
    return upgradeTypeData[this->id].gasPriceFactor;
  }
  int UpgradeType::upgradeTime() const
  {
    return upgradeTypeData[this->id].upgradeTimeBase;
  }
  int UpgradeType::upgradeTimeFactor() const
  {
    return upgradeTypeData[this->id].upgradeTimeFactor;
  }
  UnitType UpgradeType::whatUpgrades() const
  {
    return upgradeTypeData[this->id].whatUpgrades;
  }
  const std::set<UnitType>& UpgradeType::whatUses() const
  {
    return upgradeTypeData[this->id].whatUses;
  }
  int UpgradeType::maxRepeats() const
  {
    return upgradeTypeData[this->id].maxRepeats;
  }
  UpgradeType UpgradeTypes::getUpgradeType(std::string name)
  {
    fixName(&name);
    std::map<std::string, UpgradeType>::iterator i = upgradeTypeMap.find(name);
    if (i == upgradeTypeMap.end())
      return UpgradeTypes::Unknown;
    return (*i).second;
  }
  std::set<UpgradeType>& UpgradeTypes::allUpgradeTypes()
  {
    return upgradeTypeSet;
  }
}
