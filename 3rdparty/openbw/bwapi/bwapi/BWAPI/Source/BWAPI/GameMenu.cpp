#include "GameImpl.h"

#include "../Config.h"

#include <BWAPI/PlayerType.h>
#include <BWAPI/Race.h>

#include <Util/Clamp.h>

#include "../../../Debug.h"

namespace BWAPI
{
  //------------------------------------------- LOAD AUTO MENU DATA ------------------------------------------
  void GameImpl::loadAutoMenuData()
  {
    this->autoMenuManager.reloadConfig();

    // Not related to the auto-menu, but it should be loaded every time auto menu data gets reloaded
    this->seedOverride = LoadConfigInt("starcraft", "seed_override", std::numeric_limits<decltype(this->seedOverride)>::max());
    this->speedOverride = LoadConfigInt("starcraft", "speed_override", std::numeric_limits<decltype(this->speedOverride)>::min());
    this->wantDropPlayers = LoadConfigStringUCase("starcraft", "drop_players", "ON") == "ON";
  }
  //--------------------------------------------- GET LOBBY STUFF --------------------------------------------
  Race GameImpl::getMenuRace(const std::string &sChosenRace)
  {
    // Determine the current player's race
    Race race;
    if ( sChosenRace == "RANDOMTP" )
      race = rand() % 2 == 0 ? Races::Terran : Races::Protoss;
    else if ( sChosenRace == "RANDOMTZ" )
      race = rand() % 2 == 0 ? Races::Terran : Races::Zerg;
    else if ( sChosenRace == "RANDOMPZ" )
      race = rand() % 2 == 0 ? Races::Protoss : Races::Zerg;
    else if ( sChosenRace == "RANDOMTPZ" )
      race = {rand() % 3};
    else
      race = Race::getType(sChosenRace);
    return race;
  }

  //---------------------------------------------- ON MENU FRAME ---------------------------------------------
  void GameImpl::onMenuFrame()
  {
    //this function is called each frame while starcraft is in the main menu system (not in-game).
    this->inGame        = false;

    events.push_back(Event::MenuFrame());
    this->server.update();

    this->autoMenuManager.onMenuFrame();
  }
  //---------------------------------------------- CHANGE RACE -----------------------------------------------
  void GameImpl::_changeRace(int slot, BWAPI::Race race)
  {
    if ( race == Races::Unknown || race == Races::None )
      return;
    
    throw std::runtime_error("_changeRace?");
  }
}

