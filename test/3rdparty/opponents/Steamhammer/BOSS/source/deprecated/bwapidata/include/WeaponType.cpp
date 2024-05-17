#include <string>
#include <map>
#include <set>
#include <BWAPI/UnitType.h>
#include <BWAPI/WeaponType.h>
#include <BWAPI/TechType.h>
#include <BWAPI/UpgradeType.h>
#include <BWAPI/DamageType.h>
#include <BWAPI/ExplosionType.h>
#include <Util/Foreach.h>

#include "Common.h"

namespace BWAPI
{
  bool initializingWeaponType = true;
  class WeaponTypeInternal
  {
    public:
      WeaponTypeInternal() {valid = false;}
      void set(const char* name, TechType techType, int damageAmount, int damageBonus, int damageCooldown, int damageFactor, UpgradeType upgradeType, DamageType damageType, ExplosionType explosionType, int minRange, int maxRange, int innerSplashRadius, int medianSplashRadius, int outerSplashRadius, bool targetsAir, bool targetsGround, bool targetsMechanical, bool targetsOrganic, bool targetsNonBuilding, bool targetsNonRobotic, bool targetsTerrain, bool targetsOrgOrMech, bool targetsOwn, UnitType whatUses)
      {
        if (initializingWeaponType)
        {
          this->name               = name;
          this->techType           = techType;
          this->damageAmount       = damageAmount;
          this->damageBonus        = damageBonus;
          this->damageCooldown     = damageCooldown;
          this->damageFactor       = damageFactor;
          this->upgradeType        = upgradeType;
          this->damageType         = damageType;
          this->explosionType      = explosionType;
          this->minRange           = minRange;
          this->maxRange           = maxRange;
          this->innerSplashRadius  = innerSplashRadius;
          this->medianSplashRadius = medianSplashRadius;
          this->outerSplashRadius  = outerSplashRadius;
          this->targetsAir         = targetsAir;
          this->targetsGround      = targetsGround;
          this->targetsMechanical  = targetsMechanical;
          this->targetsOrganic     = targetsOrganic;
          this->targetsNonBuilding = targetsNonBuilding;
          this->targetsNonRobotic  = targetsNonRobotic;
          this->targetsTerrain     = targetsTerrain;
          this->targetsOrgOrMech   = targetsOrgOrMech;
          this->targetsOwn         = targetsOwn;
          this->whatUses           = whatUses;
          this->valid              = true;
        }
      }
      std::string name;
      TechType techType;
      UnitType whatUses;
      int damageAmount;
      int damageBonus;
      int damageCooldown;
      int damageFactor;
      UpgradeType upgradeType;
      DamageType damageType;
      ExplosionType explosionType;
      int minRange;
      int maxRange;
      int innerSplashRadius;
      int medianSplashRadius;
      int outerSplashRadius;
      bool targetsAir;
      bool targetsGround;
      bool targetsMechanical;
      bool targetsOrganic;
      bool targetsNonBuilding;
      bool targetsNonRobotic;
      bool targetsTerrain;
      bool targetsOrgOrMech;
      bool targetsOwn;
      bool valid;
  };
  WeaponTypeInternal weaponTypeData[132];
  std::map<std::string, WeaponType> weaponTypeMap;
  std::set< WeaponType > weaponTypeSet;
  std::set< WeaponType > specialWeaponTypeSet;
  std::set< WeaponType > normalWeaponTypeSet;
  namespace WeaponTypes
  {
    const WeaponType Gauss_Rifle(0);
    const WeaponType Gauss_Rifle_Jim_Raynor(1);
    const WeaponType C_10_Canister_Rifle(2);
    const WeaponType C_10_Canister_Rifle_Sarah_Kerrigan(3);
    const WeaponType C_10_Canister_Rifle_Samir_Duran(112);
    const WeaponType C_10_Canister_Rifle_Infested_Duran(113);
    const WeaponType C_10_Canister_Rifle_Alexei_Stukov(116);
    const WeaponType Fragmentation_Grenade(4);
    const WeaponType Fragmentation_Grenade_Jim_Raynor(5);
    const WeaponType Spider_Mines(6);
    const WeaponType Twin_Autocannons(7);
    const WeaponType Twin_Autocannons_Alan_Schezar(9);
    const WeaponType Hellfire_Missile_Pack(8);
    const WeaponType Hellfire_Missile_Pack_Alan_Schezar(10);
    const WeaponType Arclite_Cannon(11);
    const WeaponType Arclite_Cannon_Edmund_Duke(12);
    const WeaponType Fusion_Cutter(13);
    const WeaponType Gemini_Missiles(15);
    const WeaponType Gemini_Missiles_Tom_Kazansky(17);
    const WeaponType Burst_Lasers(16);
    const WeaponType Burst_Lasers_Tom_Kazansky(18);
    const WeaponType ATS_Laser_Battery(19);
    const WeaponType ATS_Laser_Battery_Hero(21);
    const WeaponType ATS_Laser_Battery_Hyperion(23);
    const WeaponType ATA_Laser_Battery(20);
    const WeaponType ATA_Laser_Battery_Hero(22);
    const WeaponType ATA_Laser_Battery_Hyperion(24);
    const WeaponType Flame_Thrower(25);
    const WeaponType Flame_Thrower_Gui_Montag(26);
    const WeaponType Arclite_Shock_Cannon(27);
    const WeaponType Arclite_Shock_Cannon_Edmund_Duke(28);
    const WeaponType Longbolt_Missile(29);
    const WeaponType Claws(35);
    const WeaponType Claws_Devouring_One(36);
    const WeaponType Claws_Infested_Kerrigan(37);
    const WeaponType Needle_Spines(38);
    const WeaponType Needle_Spines_Hunter_Killer(39);
    const WeaponType Kaiser_Blades(40);
    const WeaponType Kaiser_Blades_Torrasque(41);
    const WeaponType Toxic_Spores(42);
    const WeaponType Spines(43);
    const WeaponType Acid_Spore(46);
    const WeaponType Acid_Spore_Kukulza(47);
    const WeaponType Glave_Wurm(48);
    const WeaponType Glave_Wurm_Kukulza(49);
    const WeaponType Seeker_Spores(52);
    const WeaponType Subterranean_Tentacle(53);
    const WeaponType Suicide_Infested_Terran(54);
    const WeaponType Suicide_Scourge(55);
    const WeaponType Particle_Beam(62);
    const WeaponType Psi_Blades(64);
    const WeaponType Psi_Blades_Fenix(65);
    const WeaponType Phase_Disruptor(66);
    const WeaponType Phase_Disruptor_Fenix(67);
    const WeaponType Psi_Assault(69);
    const WeaponType Psionic_Shockwave(70);
    const WeaponType Psionic_Shockwave_TZ_Archon(71);
    const WeaponType Dual_Photon_Blasters(73);
    const WeaponType Dual_Photon_Blasters_Mojo(75);
    const WeaponType Dual_Photon_Blasters_Artanis(114);
    const WeaponType Anti_Matter_Missiles(74);
    const WeaponType Anti_Matter_Missiles_Mojo(76);
    const WeaponType Anti_Matter_Missiles_Artanis(115);
    const WeaponType Phase_Disruptor_Cannon(77);
    const WeaponType Phase_Disruptor_Cannon_Danimoth(78);
    const WeaponType Pulse_Cannon(79);
    const WeaponType STS_Photon_Cannon(80);
    const WeaponType STA_Photon_Cannon(81);
    const WeaponType Scarab(82);
    const WeaponType Subterranean_Spines(109);
    const WeaponType Warp_Blades(111);
    const WeaponType Warp_Blades_Hero(86);
    const WeaponType Warp_Blades_Zeratul(85);
    const WeaponType Neutron_Flare(100);
    const WeaponType Halo_Rockets(103);

    const WeaponType Yamato_Gun(30);
    const WeaponType Nuclear_Strike(31);
    const WeaponType Lockdown(32);
    const WeaponType EMP_Shockwave(33);
    const WeaponType Irradiate(34);
    const WeaponType Parasite(56);
    const WeaponType Spawn_Broodlings(57);
    const WeaponType Ensnare(58);
    const WeaponType Dark_Swarm(59);
    const WeaponType Plague(60);
    const WeaponType Consume(61);
    const WeaponType Stasis_Field(83);
    const WeaponType Psionic_Storm(84);
    const WeaponType Disruption_Web(101);
    const WeaponType Restoration(102);
    const WeaponType Corrosive_Acid(104);
    const WeaponType Mind_Control(105);
    const WeaponType Feedback(106);
    const WeaponType Optical_Flare(107);
    const WeaponType Maelstrom(108);
    const WeaponType None(130);
    const WeaponType Unknown(131);

    void init()
    {
      weaponTypeData[Gauss_Rifle.getID()].set("Gauss Rifle", TechTypes::None, 6, 1, 15, 1, UpgradeTypes::Terran_Infantry_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 128, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Marine);
      weaponTypeData[Gauss_Rifle_Jim_Raynor.getID()].set("Gauss Rifle (Jim Raynor)", TechTypes::None, 18, 1, 15, 1, UpgradeTypes::Terran_Infantry_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 160, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Jim_Raynor_Marine);
      weaponTypeData[C_10_Canister_Rifle.getID()].set("C-10 Canister Rifle", TechTypes::None, 10, 1, 22, 1, UpgradeTypes::Terran_Infantry_Weapons, DamageTypes::Concussive, ExplosionTypes::Normal, 0, 224, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Ghost);
      weaponTypeData[C_10_Canister_Rifle_Sarah_Kerrigan.getID()].set("C-10 Canister Rifle (Sarah Kerrigan)", TechTypes::None, 30, 1, 22, 1, UpgradeTypes::Terran_Infantry_Weapons, DamageTypes::Concussive, ExplosionTypes::Normal, 0, 192, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Sarah_Kerrigan);
      weaponTypeData[C_10_Canister_Rifle_Samir_Duran.getID()].set("C-10 Canister Rifle (Samir Duran)", TechTypes::None, 25, 1, 22, 1, UpgradeTypes::Terran_Infantry_Weapons, DamageTypes::Concussive, ExplosionTypes::Normal, 0, 192, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Samir_Duran);
      weaponTypeData[C_10_Canister_Rifle_Infested_Duran.getID()].set("C-10 Canister Rifle (Infested Duran)", TechTypes::None, 25, 1, 22, 1, UpgradeTypes::Terran_Infantry_Weapons, DamageTypes::Concussive, ExplosionTypes::Normal, 0, 192, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Infested_Duran);
      weaponTypeData[C_10_Canister_Rifle_Alexei_Stukov.getID()].set("C-10 Canister Rifle (Alexei Stukov)", TechTypes::None, 30, 1, 22, 1, UpgradeTypes::Terran_Infantry_Weapons, DamageTypes::Concussive, ExplosionTypes::Normal, 0, 192, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Alexei_Stukov);
      weaponTypeData[Fragmentation_Grenade.getID()].set("Fragmentation Grenade", TechTypes::None, 20, 2, 30, 1, UpgradeTypes::Terran_Vehicle_Weapons, DamageTypes::Concussive, ExplosionTypes::Normal, 0, 160, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Vulture);
      weaponTypeData[Fragmentation_Grenade_Jim_Raynor.getID()].set("Fragmentation Grenade (Jim Raynor)", TechTypes::None, 30, 2, 22, 1, UpgradeTypes::Terran_Vehicle_Weapons, DamageTypes::Concussive, ExplosionTypes::Normal, 0, 160, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Jim_Raynor_Vulture);
      weaponTypeData[Spider_Mines.getID()].set("Spider Mines", TechTypes::Spider_Mines, 125, 0, 22, 1, UpgradeTypes::None, DamageTypes::Explosive, ExplosionTypes::Radial_Splash, 0, 10, 50, 75, 100, 0, 1, 0, 0, 1, 0, 0, 0, 0, UnitTypes::Terran_Vulture_Spider_Mine);
      weaponTypeData[Twin_Autocannons.getID()].set("Twin Autocannons", TechTypes::None, 12, 1, 22, 1, UpgradeTypes::Terran_Vehicle_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 192, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Goliath);
      weaponTypeData[Twin_Autocannons_Alan_Schezar.getID()].set("Twin Autocannons (Alan Schezar)", TechTypes::None, 24, 1, 22, 1, UpgradeTypes::Terran_Vehicle_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 160, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Alan_Schezar);
      weaponTypeData[Hellfire_Missile_Pack.getID()].set("Hellfire Missile Pack", TechTypes::None, 10, 2, 22, 2, UpgradeTypes::Terran_Vehicle_Weapons, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 160, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Goliath);
      weaponTypeData[Hellfire_Missile_Pack_Alan_Schezar.getID()].set("Hellfire Missile Pack (Alan Schezar)", TechTypes::None, 20, 1, 22, 2, UpgradeTypes::Terran_Vehicle_Weapons, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 160, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Alan_Schezar);
      weaponTypeData[Arclite_Cannon.getID()].set("Arclite Cannon", TechTypes::None, 30, 3, 37, 1, UpgradeTypes::Terran_Vehicle_Weapons, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 224, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Siege_Tank_Tank_Mode);
      weaponTypeData[Arclite_Cannon_Edmund_Duke.getID()].set("Arclite Cannon (Edmund Duke)", TechTypes::None, 70, 3, 37, 1, UpgradeTypes::Terran_Vehicle_Weapons, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 224, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Edmund_Duke_Tank_Mode);
      weaponTypeData[Fusion_Cutter.getID()].set("Fusion Cutter", TechTypes::None, 5, 1, 15, 1, UpgradeTypes::None, DamageTypes::Normal, ExplosionTypes::Normal, 0, 10, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_SCV);
      weaponTypeData[Gemini_Missiles.getID()].set("Gemini Missiles", TechTypes::None, 20, 2, 22, 1, UpgradeTypes::Terran_Ship_Weapons, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 160, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Wraith);
      weaponTypeData[Gemini_Missiles_Tom_Kazansky.getID()].set("Gemini Missiles (Tom Kazansky)", TechTypes::None, 40, 2, 22, 1, UpgradeTypes::Terran_Ship_Weapons, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 160, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Tom_Kazansky);
      weaponTypeData[Burst_Lasers.getID()].set("Burst Lasers", TechTypes::None, 8, 1, 30, 1, UpgradeTypes::Terran_Ship_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 160, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Wraith);
      weaponTypeData[Burst_Lasers_Tom_Kazansky.getID()].set("Burst Lasers (Tom Kazansky)", TechTypes::None, 16, 1, 30, 1, UpgradeTypes::Terran_Ship_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 160, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Tom_Kazansky);
      weaponTypeData[ATS_Laser_Battery.getID()].set("ATS Laser Battery", TechTypes::None, 25, 3, 30, 1, UpgradeTypes::Terran_Ship_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 192, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Battlecruiser);
      weaponTypeData[ATS_Laser_Battery_Hero.getID()].set("ATS Laser Battery (Hero)", TechTypes::None, 50, 3, 30, 1, UpgradeTypes::Terran_Ship_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 192, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Norad_II);
      weaponTypeData[ATS_Laser_Battery_Hyperion.getID()].set("ATS Laser Battery (Hyperion)", TechTypes::None, 30, 3, 22, 1, UpgradeTypes::Terran_Ship_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 192, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Hyperion);
      weaponTypeData[ATA_Laser_Battery.getID()].set("ATA Laser Battery", TechTypes::None, 25, 3, 30, 1, UpgradeTypes::Terran_Ship_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 192, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Battlecruiser);
      weaponTypeData[ATA_Laser_Battery_Hero.getID()].set("ATA Laser Battery (Hero)", TechTypes::None, 50, 3, 30, 1, UpgradeTypes::Terran_Ship_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 192, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Norad_II);
      weaponTypeData[ATA_Laser_Battery_Hyperion.getID()].set("ATA Laser Battery (Hyperion)", TechTypes::None, 30, 3, 22, 1, UpgradeTypes::Terran_Ship_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 192, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Hyperion);
      weaponTypeData[Flame_Thrower.getID()].set("Flame Thrower", TechTypes::None, 8, 1, 22, 1, UpgradeTypes::Terran_Infantry_Weapons, DamageTypes::Concussive, ExplosionTypes::Enemy_Splash, 0, 32, 15, 20, 25, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Firebat);
      weaponTypeData[Flame_Thrower_Gui_Montag.getID()].set("Flame Thrower (Gui Montag)", TechTypes::None, 16, 1, 22, 1, UpgradeTypes::Terran_Infantry_Weapons, DamageTypes::Concussive, ExplosionTypes::Enemy_Splash, 0, 32, 15, 20, 25, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Firebat);
      weaponTypeData[Arclite_Shock_Cannon.getID()].set("Arclite Shock Cannon", TechTypes::None, 70, 5, 75, 1, UpgradeTypes::Terran_Vehicle_Weapons, DamageTypes::Explosive, ExplosionTypes::Radial_Splash, 64, 384, 10, 25, 40, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Siege_Tank_Siege_Mode);
      weaponTypeData[Arclite_Shock_Cannon_Edmund_Duke.getID()].set("Arclite Shock Cannon (Edmund Duke)", TechTypes::None, 150, 5, 75, 1, UpgradeTypes::Terran_Vehicle_Weapons, DamageTypes::Explosive, ExplosionTypes::Radial_Splash, 64, 384, 10, 25, 40, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Edmund_Duke_Siege_Mode);
      weaponTypeData[Longbolt_Missile.getID()].set("Longbolt Missile", TechTypes::None, 20, 0, 15, 1, UpgradeTypes::None, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 224, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Missile_Turret);
      weaponTypeData[Yamato_Gun.getID()].set("Yamato Gun", TechTypes::Yamato_Gun, 260, 0, 15, 1, UpgradeTypes::None, DamageTypes::Explosive, ExplosionTypes::Yamato_Gun, 0, 320, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Battlecruiser);
      weaponTypeData[Nuclear_Strike.getID()].set("Nuclear Strike", TechTypes::Nuclear_Strike, 600, 0, 1, 1, UpgradeTypes::None, DamageTypes::Explosive, ExplosionTypes::Nuclear_Missile, 0, 3, 128, 192, 256, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Ghost);
      weaponTypeData[Lockdown.getID()].set("Lockdown", TechTypes::Lockdown, 0, 0, 1, 1, UpgradeTypes::None, DamageTypes::Concussive, ExplosionTypes::Lockdown, 0, 256, 0, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, UnitTypes::Terran_Ghost);
      weaponTypeData[EMP_Shockwave.getID()].set("EMP Shockwave", TechTypes::EMP_Shockwave, 0, 0, 1, 1, UpgradeTypes::None, DamageTypes::Concussive, ExplosionTypes::EMP_Shockwave, 0, 256, 64, 64, 64, 1, 1, 0, 0, 0, 0, 1, 0, 0, UnitTypes::Terran_Science_Vessel);
      weaponTypeData[Irradiate.getID()].set("Irradiate", TechTypes::Irradiate, 250, 0, 75, 1, UpgradeTypes::None, DamageTypes::Ignore_Armor, ExplosionTypes::Irradiate, 0, 288, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, UnitTypes::Terran_Science_Vessel);
      weaponTypeData[Claws.getID()].set("Claws", TechTypes::None, 5, 1, 8, 1, UpgradeTypes::Zerg_Melee_Attacks, DamageTypes::Normal, ExplosionTypes::Normal, 0, 15, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Zerg_Zergling);
      weaponTypeData[Claws_Devouring_One.getID()].set("Claws (Devouring One)", TechTypes::None, 10, 1, 8, 1, UpgradeTypes::Zerg_Melee_Attacks, DamageTypes::Normal, ExplosionTypes::Normal, 0, 15, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Devouring_One);
      weaponTypeData[Claws_Infested_Kerrigan.getID()].set("Claws (Infested Kerrigan)", TechTypes::None, 50, 1, 15, 1, UpgradeTypes::Zerg_Melee_Attacks, DamageTypes::Normal, ExplosionTypes::Normal, 0, 15, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Infested_Kerrigan);
      weaponTypeData[Needle_Spines.getID()].set("Needle Spines", TechTypes::None, 10, 1, 15, 1, UpgradeTypes::Zerg_Missile_Attacks, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 128, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Zerg_Hydralisk);
      weaponTypeData[Needle_Spines_Hunter_Killer.getID()].set("Needle Spines (Hunter Killer)", TechTypes::None, 20, 1, 15, 1, UpgradeTypes::Zerg_Missile_Attacks, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 160, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Hunter_Killer);
      weaponTypeData[Kaiser_Blades.getID()].set("Kaiser Blades", TechTypes::None, 20, 3, 15, 1, UpgradeTypes::Zerg_Melee_Attacks, DamageTypes::Normal, ExplosionTypes::Normal, 0, 25, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Zerg_Ultralisk);
      weaponTypeData[Kaiser_Blades_Torrasque.getID()].set("Kaiser Blades (Torrasque)", TechTypes::None, 50, 3, 15, 1, UpgradeTypes::Zerg_Melee_Attacks, DamageTypes::Normal, ExplosionTypes::Normal, 0, 25, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Torrasque);
      weaponTypeData[Toxic_Spores.getID()].set("Toxic Spores", TechTypes::None, 4, 1, 15, 1, UpgradeTypes::Zerg_Melee_Attacks, DamageTypes::Normal, ExplosionTypes::Normal, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Zerg_Broodling);
      weaponTypeData[Spines.getID()].set("Spines", TechTypes::None, 5, 0, 22, 1, UpgradeTypes::None, DamageTypes::Normal, ExplosionTypes::Normal, 0, 32, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Zerg_Drone);
      weaponTypeData[Acid_Spore.getID()].set("Acid Spore", TechTypes::None, 20, 2, 30, 1, UpgradeTypes::Zerg_Flyer_Attacks, DamageTypes::Normal, ExplosionTypes::Normal, 0, 256, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Zerg_Guardian);
      weaponTypeData[Acid_Spore_Kukulza.getID()].set("Acid Spore (Kukulza)", TechTypes::None, 40, 2, 30, 1, UpgradeTypes::Zerg_Flyer_Attacks, DamageTypes::Normal, ExplosionTypes::Normal, 0, 256, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Kukulza_Guardian);
      weaponTypeData[Glave_Wurm.getID()].set("Glave Wurm", TechTypes::None, 9, 1, 30, 1, UpgradeTypes::Zerg_Flyer_Attacks, DamageTypes::Normal, ExplosionTypes::Normal, 0, 96, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Zerg_Mutalisk);
      weaponTypeData[Glave_Wurm_Kukulza.getID()].set("Glave Wurm (Kukulza)", TechTypes::None, 18, 1, 30, 1, UpgradeTypes::Zerg_Flyer_Attacks, DamageTypes::Normal, ExplosionTypes::Normal, 0, 96, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Kukulza_Mutalisk);
      weaponTypeData[Seeker_Spores.getID()].set("Seeker Spores", TechTypes::None, 15, 0, 15, 1, UpgradeTypes::None, DamageTypes::Normal, ExplosionTypes::Normal, 0, 224, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Zerg_Spore_Colony);
      weaponTypeData[Subterranean_Tentacle.getID()].set("Subterranean Tentacle", TechTypes::None, 40, 0, 32, 1, UpgradeTypes::None, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 224, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Zerg_Sunken_Colony);
      weaponTypeData[Suicide_Infested_Terran.getID()].set("Suicide Infested Terran", TechTypes::None, 500, 0, 1, 1, UpgradeTypes::None, DamageTypes::Explosive, ExplosionTypes::Radial_Splash, 0, 3, 20, 40, 60, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Zerg_Infested_Terran);
      weaponTypeData[Suicide_Scourge.getID()].set("Suicide Scourge", TechTypes::None, 110, 0, 1, 1, UpgradeTypes::None, DamageTypes::Normal, ExplosionTypes::Normal, 0, 3, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Zerg_Scourge);
      weaponTypeData[Parasite.getID()].set("Parasite", TechTypes::Parasite, 0, 0, 1, 1, UpgradeTypes::None, DamageTypes::Independent, ExplosionTypes::Parasite, 0, 384, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, UnitTypes::Zerg_Queen);
      weaponTypeData[Spawn_Broodlings.getID()].set("Spawn Broodlings", TechTypes::Spawn_Broodlings, 0, 0, 1, 1, UpgradeTypes::None, DamageTypes::Independent, ExplosionTypes::Broodlings, 0, 288, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, UnitTypes::Zerg_Queen);
      weaponTypeData[Ensnare.getID()].set("Ensnare", TechTypes::Ensnare, 0, 0, 1, 1, UpgradeTypes::None, DamageTypes::Independent, ExplosionTypes::Ensnare, 0, 288, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, UnitTypes::Zerg_Queen);
      weaponTypeData[Dark_Swarm.getID()].set("Dark Swarm", TechTypes::Dark_Swarm, 0, 0, 1, 1, UpgradeTypes::None, DamageTypes::Independent, ExplosionTypes::Dark_Swarm, 0, 288, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Zerg_Defiler);
      weaponTypeData[Plague.getID()].set("Plague", TechTypes::Plague, 300, 0, 1, 1, UpgradeTypes::None, DamageTypes::Independent, ExplosionTypes::Plague, 0, 288, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, UnitTypes::Zerg_Defiler);
      weaponTypeData[Consume.getID()].set("Consume", TechTypes::Consume, 0, 0, 1, 1, UpgradeTypes::None, DamageTypes::Independent, ExplosionTypes::Consume, 0, 16, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, UnitTypes::Zerg_Defiler);
      weaponTypeData[Particle_Beam.getID()].set("Particle Beam", TechTypes::None, 5, 0, 22, 1, UpgradeTypes::None, DamageTypes::Normal, ExplosionTypes::Normal, 0, 32, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Protoss_Probe);
      weaponTypeData[Psi_Blades.getID()].set("Psi Blades", TechTypes::None, 8, 1, 22, 1, UpgradeTypes::Protoss_Ground_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 15, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Protoss_Zealot);
      weaponTypeData[Psi_Blades_Fenix.getID()].set("Psi Blades (Fenix)", TechTypes::None, 20, 1, 22, 1, UpgradeTypes::Protoss_Ground_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 15, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Fenix_Zealot);
      weaponTypeData[Phase_Disruptor.getID()].set("Phase Disruptor", TechTypes::None, 20, 2, 30, 1, UpgradeTypes::Protoss_Ground_Weapons, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 128, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Protoss_Dragoon);
      weaponTypeData[Phase_Disruptor_Fenix.getID()].set("Phase Disruptor (Fenix)", TechTypes::None, 45, 2, 22, 1, UpgradeTypes::Protoss_Ground_Weapons, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 128, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Fenix_Dragoon);
      weaponTypeData[Psi_Assault.getID()].set("Psi Assault", TechTypes::None, 20, 1, 22, 1, UpgradeTypes::Protoss_Ground_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 96, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Tassadar);
      weaponTypeData[Psionic_Shockwave.getID()].set("Psionic Shockwave", TechTypes::None, 30, 3, 20, 1, UpgradeTypes::Protoss_Ground_Weapons, DamageTypes::Normal, ExplosionTypes::Enemy_Splash, 0, 64, 3, 15, 30, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Protoss_Archon);
      weaponTypeData[Psionic_Shockwave_TZ_Archon.getID()].set("Psionic Shockwave (Tassadar/Zeratul Archon)", TechTypes::None, 60, 3, 20, 1, UpgradeTypes::Protoss_Ground_Weapons, DamageTypes::Normal, ExplosionTypes::Enemy_Splash, 0, 64, 3, 15, 30, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Tassadar_Zeratul_Archon);
      weaponTypeData[Dual_Photon_Blasters.getID()].set("Dual Photon Blasters", TechTypes::None, 8, 1, 30, 1, UpgradeTypes::Protoss_Air_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 128, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Protoss_Scout);
      weaponTypeData[Dual_Photon_Blasters_Mojo.getID()].set("Dual Photon Blasters (Mojo)", TechTypes::None, 20, 1, 30, 1, UpgradeTypes::Protoss_Air_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 128, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Mojo);
      weaponTypeData[Dual_Photon_Blasters_Artanis.getID()].set("Dual Photon Blasters (Artanis)", TechTypes::None, 20, 1, 30, 1, UpgradeTypes::Protoss_Air_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 128, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Artanis);
      weaponTypeData[Anti_Matter_Missiles.getID()].set("Anti-Matter Missiles", TechTypes::None, 14, 1, 22, 2, UpgradeTypes::Protoss_Air_Weapons, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 128, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Protoss_Scout);
      weaponTypeData[Anti_Matter_Missiles_Mojo.getID()].set("Anti-Matter Missiles (Mojo)", TechTypes::None, 28, 1, 22, 2, UpgradeTypes::Protoss_Air_Weapons, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 128, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Mojo);
      weaponTypeData[Anti_Matter_Missiles_Artanis.getID()].set("Anti-Matter Missiles (Artanis)", TechTypes::None, 28, 1, 22, 2, UpgradeTypes::Protoss_Air_Weapons, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 128, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Artanis);
      weaponTypeData[Phase_Disruptor_Cannon.getID()].set("Phase Disruptor Cannon", TechTypes::None, 10, 1, 45, 1, UpgradeTypes::Protoss_Air_Weapons, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 160, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Protoss_Arbiter);
      weaponTypeData[Phase_Disruptor_Cannon_Danimoth.getID()].set("Phase Disruptor Cannon (Danimoth)", TechTypes::None, 20, 1, 45, 1, UpgradeTypes::Protoss_Air_Weapons, DamageTypes::Explosive, ExplosionTypes::Normal, 0, 160, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Danimoth);
      weaponTypeData[Pulse_Cannon.getID()].set("Pulse Cannon", TechTypes::None, 6, 1, 1, 1, UpgradeTypes::Protoss_Air_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 128, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Protoss_Interceptor);
      weaponTypeData[STS_Photon_Cannon.getID()].set("STS Photon Cannon", TechTypes::None, 20, 0, 22, 1, UpgradeTypes::None, DamageTypes::Normal, ExplosionTypes::Normal, 0, 224, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Protoss_Photon_Cannon);
      weaponTypeData[STA_Photon_Cannon.getID()].set("STA Photon Cannon", TechTypes::None, 20, 0, 22, 1, UpgradeTypes::None, DamageTypes::Normal, ExplosionTypes::Normal, 0, 224, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Protoss_Photon_Cannon);
      weaponTypeData[Scarab.getID()].set("Scarab", TechTypes::None, 100, 25, 1, 1, UpgradeTypes::Scarab_Damage, DamageTypes::Normal, ExplosionTypes::Enemy_Splash, 0, 128, 20, 40, 60, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Protoss_Scarab);
      weaponTypeData[Stasis_Field.getID()].set("Stasis Field", TechTypes::Stasis_Field, 0, 1, 1, 1, UpgradeTypes::None, DamageTypes::Independent, ExplosionTypes::Stasis_Field, 0, 288, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, UnitTypes::Protoss_Arbiter);
      weaponTypeData[Psionic_Storm.getID()].set("Psionic Storm", TechTypes::Psionic_Storm, 14, 1, 45, 1, UpgradeTypes::None, DamageTypes::Ignore_Armor, ExplosionTypes::Radial_Splash, 0, 288, 48, 48, 48, 1, 1, 0, 0, 1, 0, 1, 0, 0, UnitTypes::Protoss_High_Templar);
      weaponTypeData[Neutron_Flare.getID()].set("Neutron Flare", TechTypes::None, 5, 1, 8, 1, UpgradeTypes::Protoss_Air_Weapons, DamageTypes::Explosive, ExplosionTypes::Air_Splash, 0, 160, 5, 50, 100, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Protoss_Corsair);
      weaponTypeData[Disruption_Web.getID()].set("Disruption Web", TechTypes::Disruption_Web, 0, 0, 22, 1, UpgradeTypes::None, DamageTypes::Ignore_Armor, ExplosionTypes::Disruption_Web, 0, 288, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Protoss_Corsair);
      weaponTypeData[Restoration.getID()].set("Restoration", TechTypes::Restoration, 20, 0, 22, 1, UpgradeTypes::None, DamageTypes::Ignore_Armor, ExplosionTypes::Restoration, 0, 192, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, UnitTypes::Terran_Medic);
      weaponTypeData[Halo_Rockets.getID()].set("Halo Rockets", TechTypes::None, 6, 1, 64, 2, UpgradeTypes::Terran_Ship_Weapons, DamageTypes::Explosive, ExplosionTypes::Air_Splash, 0, 192, 5, 50, 100, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Valkyrie);
      weaponTypeData[Corrosive_Acid.getID()].set("Corrosive Acid", TechTypes::None, 25, 2, 100, 1, UpgradeTypes::Zerg_Flyer_Attacks, DamageTypes::Explosive, ExplosionTypes::Corrosive_Acid, 0, 192, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Zerg_Devourer);
      weaponTypeData[Mind_Control.getID()].set("Mind Control", TechTypes::Mind_Control, 8, 1, 22, 1, UpgradeTypes::None, DamageTypes::Normal, ExplosionTypes::Mind_Control, 0, 256, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, UnitTypes::Protoss_Dark_Archon);
      weaponTypeData[Feedback.getID()].set("Feedback", TechTypes::Feedback, 8, 1, 22, 1, UpgradeTypes::None, DamageTypes::Ignore_Armor, ExplosionTypes::Feedback, 0, 320, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, UnitTypes::Protoss_Dark_Archon);
      weaponTypeData[Optical_Flare.getID()].set("Optical Flare", TechTypes::Optical_Flare, 8, 1, 22, 1, UpgradeTypes::None, DamageTypes::Independent, ExplosionTypes::Optical_Flare, 0, 288, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Terran_Medic);
      weaponTypeData[Maelstrom.getID()].set("Maelstrom", TechTypes::Maelstrom, 0, 1, 1, 1, UpgradeTypes::None, DamageTypes::Independent, ExplosionTypes::Maelstrom, 0, 320, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, UnitTypes::Protoss_Dark_Archon);
      weaponTypeData[Subterranean_Spines.getID()].set("Subterranean Spines", TechTypes::None, 20, 2, 37, 1, UpgradeTypes::Zerg_Missile_Attacks, DamageTypes::Normal, ExplosionTypes::Enemy_Splash, 0, 192, 20, 20, 20, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Zerg_Lurker);
      weaponTypeData[Warp_Blades.getID()].set("Warp Blades", TechTypes::None, 40, 3, 30, 1, UpgradeTypes::Protoss_Ground_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 15, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Protoss_Dark_Templar);
      weaponTypeData[Warp_Blades_Hero.getID()].set("Warp Blades (Hero)", TechTypes::None, 45, 1, 30, 1, UpgradeTypes::Protoss_Ground_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 15, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Dark_Templar);
      weaponTypeData[Warp_Blades_Zeratul.getID()].set("Warp Blades (Zeratul)", TechTypes::None, 100, 1, 22, 1, UpgradeTypes::Protoss_Ground_Weapons, DamageTypes::Normal, ExplosionTypes::Normal, 0, 15, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, UnitTypes::Hero_Zeratul);
      weaponTypeData[None.getID()].set("None", TechTypes::None, 0, 0, 0, 0, UpgradeTypes::None, DamageTypes::None, ExplosionTypes::None, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::None);
      weaponTypeData[Unknown.getID()].set("Unknown", TechTypes::None, 0, 0, 0, 0, UpgradeTypes::None, DamageTypes::None, ExplosionTypes::None, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, UnitTypes::None);

      weaponTypeSet.insert(Gauss_Rifle);
      weaponTypeSet.insert(Gauss_Rifle_Jim_Raynor);
      weaponTypeSet.insert(C_10_Canister_Rifle);
      weaponTypeSet.insert(C_10_Canister_Rifle_Sarah_Kerrigan);
      weaponTypeSet.insert(C_10_Canister_Rifle_Samir_Duran);
      weaponTypeSet.insert(C_10_Canister_Rifle_Infested_Duran);
      weaponTypeSet.insert(C_10_Canister_Rifle_Alexei_Stukov);
      weaponTypeSet.insert(Fragmentation_Grenade);
      weaponTypeSet.insert(Fragmentation_Grenade_Jim_Raynor);
      weaponTypeSet.insert(Spider_Mines);
      weaponTypeSet.insert(Twin_Autocannons);
      weaponTypeSet.insert(Twin_Autocannons_Alan_Schezar);
      weaponTypeSet.insert(Hellfire_Missile_Pack);
      weaponTypeSet.insert(Hellfire_Missile_Pack_Alan_Schezar);
      weaponTypeSet.insert(Arclite_Cannon);
      weaponTypeSet.insert(Arclite_Cannon_Edmund_Duke);
      weaponTypeSet.insert(Fusion_Cutter);
      weaponTypeSet.insert(Gemini_Missiles);
      weaponTypeSet.insert(Gemini_Missiles_Tom_Kazansky);
      weaponTypeSet.insert(Burst_Lasers);
      weaponTypeSet.insert(Burst_Lasers_Tom_Kazansky);
      weaponTypeSet.insert(ATS_Laser_Battery);
      weaponTypeSet.insert(ATS_Laser_Battery_Hero);
      weaponTypeSet.insert(ATS_Laser_Battery_Hyperion);
      weaponTypeSet.insert(ATA_Laser_Battery);
      weaponTypeSet.insert(ATA_Laser_Battery_Hero);
      weaponTypeSet.insert(ATA_Laser_Battery_Hyperion);
      weaponTypeSet.insert(Flame_Thrower);
      weaponTypeSet.insert(Flame_Thrower_Gui_Montag);
      weaponTypeSet.insert(Arclite_Shock_Cannon);
      weaponTypeSet.insert(Arclite_Shock_Cannon_Edmund_Duke);
      weaponTypeSet.insert(Longbolt_Missile);
      weaponTypeSet.insert(Claws);
      weaponTypeSet.insert(Claws_Devouring_One);
      weaponTypeSet.insert(Claws_Infested_Kerrigan);
      weaponTypeSet.insert(Needle_Spines);
      weaponTypeSet.insert(Needle_Spines_Hunter_Killer);
      weaponTypeSet.insert(Kaiser_Blades);
      weaponTypeSet.insert(Kaiser_Blades_Torrasque);
      weaponTypeSet.insert(Toxic_Spores);
      weaponTypeSet.insert(Spines);
      weaponTypeSet.insert(Acid_Spore);
      weaponTypeSet.insert(Acid_Spore_Kukulza);
      weaponTypeSet.insert(Glave_Wurm);
      weaponTypeSet.insert(Glave_Wurm_Kukulza);
      weaponTypeSet.insert(Seeker_Spores);
      weaponTypeSet.insert(Subterranean_Tentacle);
      weaponTypeSet.insert(Suicide_Infested_Terran);
      weaponTypeSet.insert(Suicide_Scourge);
      weaponTypeSet.insert(Particle_Beam);
      weaponTypeSet.insert(Psi_Blades);
      weaponTypeSet.insert(Psi_Blades_Fenix);
      weaponTypeSet.insert(Phase_Disruptor);
      weaponTypeSet.insert(Phase_Disruptor_Fenix);
      weaponTypeSet.insert(Psi_Assault);
      weaponTypeSet.insert(Psionic_Shockwave);
      weaponTypeSet.insert(Psionic_Shockwave_TZ_Archon);
      weaponTypeSet.insert(Dual_Photon_Blasters);
      weaponTypeSet.insert(Dual_Photon_Blasters_Mojo);
      weaponTypeSet.insert(Dual_Photon_Blasters_Artanis);
      weaponTypeSet.insert(Anti_Matter_Missiles);
      weaponTypeSet.insert(Anti_Matter_Missiles_Mojo);
      weaponTypeSet.insert(Anti_Matter_Missiles_Artanis);
      weaponTypeSet.insert(Phase_Disruptor_Cannon);
      weaponTypeSet.insert(Phase_Disruptor_Cannon_Danimoth);
      weaponTypeSet.insert(Pulse_Cannon);
      weaponTypeSet.insert(STS_Photon_Cannon);
      weaponTypeSet.insert(STA_Photon_Cannon);
      weaponTypeSet.insert(Scarab);
      weaponTypeSet.insert(Neutron_Flare);
      weaponTypeSet.insert(Halo_Rockets);
      weaponTypeSet.insert(Corrosive_Acid);
      weaponTypeSet.insert(Subterranean_Spines);
      weaponTypeSet.insert(Warp_Blades);
      weaponTypeSet.insert(Warp_Blades_Hero);
      weaponTypeSet.insert(Warp_Blades_Zeratul);

      normalWeaponTypeSet.insert(Gauss_Rifle);
      normalWeaponTypeSet.insert(Gauss_Rifle_Jim_Raynor);
      normalWeaponTypeSet.insert(C_10_Canister_Rifle);
      normalWeaponTypeSet.insert(C_10_Canister_Rifle_Sarah_Kerrigan);
      normalWeaponTypeSet.insert(C_10_Canister_Rifle_Samir_Duran);
      normalWeaponTypeSet.insert(C_10_Canister_Rifle_Infested_Duran);
      normalWeaponTypeSet.insert(C_10_Canister_Rifle_Alexei_Stukov);
      normalWeaponTypeSet.insert(Fragmentation_Grenade);
      normalWeaponTypeSet.insert(Fragmentation_Grenade_Jim_Raynor);
      normalWeaponTypeSet.insert(Spider_Mines);
      normalWeaponTypeSet.insert(Twin_Autocannons);
      normalWeaponTypeSet.insert(Twin_Autocannons_Alan_Schezar);
      normalWeaponTypeSet.insert(Hellfire_Missile_Pack);
      normalWeaponTypeSet.insert(Hellfire_Missile_Pack_Alan_Schezar);
      normalWeaponTypeSet.insert(Arclite_Cannon);
      normalWeaponTypeSet.insert(Arclite_Cannon_Edmund_Duke);
      normalWeaponTypeSet.insert(Fusion_Cutter);
      normalWeaponTypeSet.insert(Gemini_Missiles);
      normalWeaponTypeSet.insert(Gemini_Missiles_Tom_Kazansky);
      normalWeaponTypeSet.insert(Burst_Lasers);
      normalWeaponTypeSet.insert(Burst_Lasers_Tom_Kazansky);
      normalWeaponTypeSet.insert(ATS_Laser_Battery);
      normalWeaponTypeSet.insert(ATS_Laser_Battery_Hero);
      normalWeaponTypeSet.insert(ATS_Laser_Battery_Hyperion);
      normalWeaponTypeSet.insert(ATA_Laser_Battery);
      normalWeaponTypeSet.insert(ATA_Laser_Battery_Hero);
      normalWeaponTypeSet.insert(ATA_Laser_Battery_Hyperion);
      normalWeaponTypeSet.insert(Flame_Thrower);
      normalWeaponTypeSet.insert(Flame_Thrower_Gui_Montag);
      normalWeaponTypeSet.insert(Arclite_Shock_Cannon);
      normalWeaponTypeSet.insert(Arclite_Shock_Cannon_Edmund_Duke);
      normalWeaponTypeSet.insert(Longbolt_Missile);
      normalWeaponTypeSet.insert(Claws);
      normalWeaponTypeSet.insert(Claws_Devouring_One);
      normalWeaponTypeSet.insert(Claws_Infested_Kerrigan);
      normalWeaponTypeSet.insert(Needle_Spines);
      normalWeaponTypeSet.insert(Needle_Spines_Hunter_Killer);
      normalWeaponTypeSet.insert(Kaiser_Blades);
      normalWeaponTypeSet.insert(Kaiser_Blades_Torrasque);
      normalWeaponTypeSet.insert(Toxic_Spores);
      normalWeaponTypeSet.insert(Spines);
      normalWeaponTypeSet.insert(Acid_Spore);
      normalWeaponTypeSet.insert(Acid_Spore_Kukulza);
      normalWeaponTypeSet.insert(Glave_Wurm);
      normalWeaponTypeSet.insert(Glave_Wurm_Kukulza);
      normalWeaponTypeSet.insert(Seeker_Spores);
      normalWeaponTypeSet.insert(Subterranean_Tentacle);
      normalWeaponTypeSet.insert(Suicide_Infested_Terran);
      normalWeaponTypeSet.insert(Suicide_Scourge);
      normalWeaponTypeSet.insert(Particle_Beam);
      normalWeaponTypeSet.insert(Psi_Blades);
      normalWeaponTypeSet.insert(Psi_Blades_Fenix);
      normalWeaponTypeSet.insert(Phase_Disruptor);
      normalWeaponTypeSet.insert(Phase_Disruptor_Fenix);
      normalWeaponTypeSet.insert(Psi_Assault);
      normalWeaponTypeSet.insert(Psionic_Shockwave);
      normalWeaponTypeSet.insert(Psionic_Shockwave_TZ_Archon);
      normalWeaponTypeSet.insert(Dual_Photon_Blasters);
      normalWeaponTypeSet.insert(Dual_Photon_Blasters_Mojo);
      normalWeaponTypeSet.insert(Dual_Photon_Blasters_Artanis);
      normalWeaponTypeSet.insert(Anti_Matter_Missiles);
      normalWeaponTypeSet.insert(Anti_Matter_Missiles_Mojo);
      normalWeaponTypeSet.insert(Anti_Matter_Missiles_Artanis);
      normalWeaponTypeSet.insert(Phase_Disruptor_Cannon);
      normalWeaponTypeSet.insert(Phase_Disruptor_Cannon_Danimoth);
      normalWeaponTypeSet.insert(Pulse_Cannon);
      normalWeaponTypeSet.insert(STS_Photon_Cannon);
      normalWeaponTypeSet.insert(STA_Photon_Cannon);
      normalWeaponTypeSet.insert(Scarab);
      normalWeaponTypeSet.insert(Neutron_Flare);
      normalWeaponTypeSet.insert(Halo_Rockets);
      normalWeaponTypeSet.insert(Corrosive_Acid);
      normalWeaponTypeSet.insert(Subterranean_Spines);
      normalWeaponTypeSet.insert(Warp_Blades);
      normalWeaponTypeSet.insert(Warp_Blades_Hero);
      normalWeaponTypeSet.insert(Warp_Blades_Zeratul);

      weaponTypeSet.insert(Yamato_Gun);
      weaponTypeSet.insert(Nuclear_Strike);
      weaponTypeSet.insert(Lockdown);
      weaponTypeSet.insert(EMP_Shockwave);
      weaponTypeSet.insert(Irradiate);
      weaponTypeSet.insert(Parasite);
      weaponTypeSet.insert(Spawn_Broodlings);
      weaponTypeSet.insert(Ensnare);
      weaponTypeSet.insert(Dark_Swarm);
      weaponTypeSet.insert(Plague);
      weaponTypeSet.insert(Consume);
      weaponTypeSet.insert(Stasis_Field);
      weaponTypeSet.insert(Psionic_Storm);
      weaponTypeSet.insert(Disruption_Web);
      weaponTypeSet.insert(Restoration);
      weaponTypeSet.insert(Mind_Control);
      weaponTypeSet.insert(Feedback);
      weaponTypeSet.insert(Optical_Flare);
      weaponTypeSet.insert(Maelstrom);

      specialWeaponTypeSet.insert(Yamato_Gun);
      specialWeaponTypeSet.insert(Nuclear_Strike);
      specialWeaponTypeSet.insert(Lockdown);
      specialWeaponTypeSet.insert(EMP_Shockwave);
      specialWeaponTypeSet.insert(Irradiate);
      specialWeaponTypeSet.insert(Parasite);
      specialWeaponTypeSet.insert(Spawn_Broodlings);
      specialWeaponTypeSet.insert(Ensnare);
      specialWeaponTypeSet.insert(Dark_Swarm);
      specialWeaponTypeSet.insert(Plague);
      specialWeaponTypeSet.insert(Consume);
      specialWeaponTypeSet.insert(Stasis_Field);
      specialWeaponTypeSet.insert(Psionic_Storm);
      specialWeaponTypeSet.insert(Disruption_Web);
      specialWeaponTypeSet.insert(Restoration);
      specialWeaponTypeSet.insert(Mind_Control);
      specialWeaponTypeSet.insert(Feedback);
      specialWeaponTypeSet.insert(Optical_Flare);
      specialWeaponTypeSet.insert(Maelstrom);

      weaponTypeSet.insert(None);
      weaponTypeSet.insert(Unknown);

      foreach(WeaponType i, weaponTypeSet)
      {
        std::string name = i.getName();
        fixName(&name);
        weaponTypeMap.insert(std::make_pair(name, i));
      }
      initializingWeaponType = false;
    }
  }
  WeaponType::WeaponType()
  {
    this->id = WeaponTypes::None.id;
  }
  WeaponType::WeaponType(int id)
  {
    this->id = id;
    if (!initializingWeaponType && (id < 0 || id >= 132 || !weaponTypeData[id].valid))
      this->id = WeaponTypes::Unknown.id;
  }
  WeaponType::WeaponType(const WeaponType& other)
  {
    this->id = other.id;
  }
  WeaponType& WeaponType::operator=(const WeaponType& other)
  {
    this->id = other.id;
    return *this;
  }
  bool WeaponType::operator==(const WeaponType& other) const
  {
    return this->id == other.id;
  }
  bool WeaponType::operator!=(const WeaponType& other) const
  {
    return this->id != other.id;
  }
  bool WeaponType::operator<(const WeaponType& other) const
  {
    return this->id < other.id;
  }
  int WeaponType::getID() const
  {
    return this->id;
  }
  std::string WeaponType::getName() const
  {
    return weaponTypeData[this->id].name;
  }
  TechType WeaponType::getTech() const
  {
    return weaponTypeData[this->id].techType;
  }
  UnitType WeaponType::whatUses() const
  {
    return weaponTypeData[this->id].whatUses;
  }
  int WeaponType::damageAmount() const
  {
    return weaponTypeData[this->id].damageAmount;
  }
  int WeaponType::damageBonus() const
  {
    return weaponTypeData[this->id].damageBonus;
  }
  int WeaponType::damageCooldown() const
  {
    return weaponTypeData[this->id].damageCooldown;
  }
  int WeaponType::damageFactor() const
  {
    return weaponTypeData[this->id].damageFactor;
  }
  UpgradeType WeaponType::upgradeType() const
  {
    return weaponTypeData[this->id].upgradeType;
  }
  DamageType WeaponType::damageType() const
  {
    return weaponTypeData[this->id].damageType;
  }
  ExplosionType WeaponType::explosionType() const
  {
    return weaponTypeData[this->id].explosionType;
  }
  int WeaponType::minRange() const
  {
    return weaponTypeData[this->id].minRange;
  }
  int WeaponType::maxRange() const
  {
    return weaponTypeData[this->id].maxRange;
  }
  int WeaponType::innerSplashRadius() const
  {
    return weaponTypeData[this->id].innerSplashRadius;
  }
  int WeaponType::medianSplashRadius() const
  {
    return weaponTypeData[this->id].medianSplashRadius;
  }
  int WeaponType::outerSplashRadius() const
  {
    return weaponTypeData[this->id].outerSplashRadius;
  }
  bool WeaponType::targetsAir() const
  {
    return weaponTypeData[this->id].targetsAir;
  }
  bool WeaponType::targetsGround() const
  {
    return weaponTypeData[this->id].targetsGround;
  }
  bool WeaponType::targetsMechanical() const
  {
    return weaponTypeData[this->id].targetsMechanical;
  }
  bool WeaponType::targetsOrganic() const
  {
    return weaponTypeData[this->id].targetsOrganic;
  }
  bool WeaponType::targetsNonBuilding() const
  {
    return weaponTypeData[this->id].targetsNonBuilding;
  }
  bool WeaponType::targetsNonRobotic() const
  {
    return weaponTypeData[this->id].targetsNonRobotic;
  }
  bool WeaponType::targetsTerrain() const
  {
    return weaponTypeData[this->id].targetsTerrain;
  }
  bool WeaponType::targetsOrgOrMech() const
  {
    return weaponTypeData[this->id].targetsOrgOrMech;
  }
  bool WeaponType::targetsOwn() const
  {
    return weaponTypeData[this->id].targetsOwn;
  }
  WeaponType WeaponTypes::getWeaponType(std::string name)
  {
    fixName(&name);
    std::map<std::string, WeaponType>::iterator i = weaponTypeMap.find(name);
    if (i == weaponTypeMap.end())
      return WeaponTypes::Unknown;
    return (*i).second;
  }
  std::set<WeaponType>& WeaponTypes::allWeaponTypes()
  {
    return weaponTypeSet;
  }
  std::set<WeaponType>& WeaponTypes::normalWeaponTypes()
  {
    return weaponTypeSet;
  }
  std::set<WeaponType>& WeaponTypes::specialWeaponTypes()
  {
    return weaponTypeSet;
  }
}
