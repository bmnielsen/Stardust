#include <BWAPI.h>

namespace BWAPI
{
    Game * Broodwar;
}

void BWAPI::BWAPI_init()
{
  BWAPI::Races::init();
  BWAPI::DamageTypes::init();
  BWAPI::ExplosionTypes::init();
  BWAPI::Orders::init();
  BWAPI::TechTypes::init();
  BWAPI::PlayerTypes::init();
  BWAPI::UpgradeTypes::init();
  BWAPI::WeaponTypes::init();
  BWAPI::UnitSizeTypes::init();
  BWAPI::UnitCommandTypes::init();
  BWAPI::UnitTypes::init();
  BWAPI::BulletTypes::init();
  BWAPI::Errors::init();
  BWAPI::Colors::init();
  BWAPI::GameTypes::init();
}
