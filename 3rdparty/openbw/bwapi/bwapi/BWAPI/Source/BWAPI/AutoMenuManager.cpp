#include "AutoMenuManager.h"
#include "../Config.h"
//#include "../DLLMain.h"

#include <algorithm>
#include <sstream>
//#include <Util/Path.h>
#include <Util/StringUtil.h>

//#include <BW/MenuPosition.h>
//#include <BW/Dialog.h>
#include <BWAPI/GameImpl.h>
//#include <BW/Offsets.h>

using namespace BWAPI;

AutoMenuManager::AutoMenuManager(BW::Game bwgame) : bwgame(bwgame)
{
  this->reloadConfig();
}

void AutoMenuManager::reloadConfig()
{
  InitPrimaryConfig();

  // Reset menu activation variables
  this->actRaceSel = false;
  this->actStartedGame = false;
  this->autoMapTryCount = 0;

  //this function is called when starcraft loads and at the end of each match.
  //the function loads the parameters for the auto-menu feature such as auto_menu, map, race, enemy_race, enemy_count, and game_type
  this->autoMenuMode = LoadConfigStringUCase("auto_menu", "auto_menu", "OFF");
#ifdef _DEBUG
  this->autoMenuPause = LoadConfigStringUCase("auto_menu", "pause_dbg", "OFF");
#endif
  this->autoMenuRestartGame = LoadConfigStringUCase("auto_menu", "auto_restart", "OFF");
  this->autoMenuGameName = LoadConfigString("auto_menu", "game");
  this->autoMenuCharacterName = LoadConfigString("auto_menu", "character_name", "FIRST").substr(0, 24);

  // Load map string
  std::string cfgMap = LoadConfigString("auto_menu", "map", "");
  std::replace(cfgMap.begin(), cfgMap.end(), '\\', '/');

  // Used to check if map string was changed.
  //static std::string lastAutoMapString;
  std::string lastAutoMapString;
  bool mapChanged = false;

  // If the auto-menu map field was changed

  if (lastAutoMapString != cfgMap)
  {
    lastAutoMapString = cfgMap;
    this->lastAutoMapEntry = 0;
    this->lastMapGen.clear();
    this->autoMapPool.clear();

    // Get just the directory
    this->autoMenuMapPath.clear();
    size_t tmp = cfgMap.find_last_of("\\/\n");
    if (tmp != std::string::npos)
      this->autoMenuMapPath = cfgMap.substr(0, tmp);
    if (autoMenuMapPath.empty()) this->autoMenuMapPath = "./";
    else this->autoMenuMapPath += "/";

//#ifdef _WIN32
#if 0
    // Iterate files in directory
    WIN32_FIND_DATAA finder = { 0 };
    HANDLE hFind = FindFirstFileA(cfgMap.c_str(), &finder);

    if (hFind != INVALID_HANDLE_VALUE)
    {
      do
      {
        if (!(finder.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))  // Check if found is not a directory
        {
          // Convert to string and add to autoMapPool if the type is valid
          std::string finderStr = std::string(finder.cFileName);
          if (getFileType(this->autoMenuMapPath + finderStr))
          {
            this->autoMapPool.push_back(finderStr);
          }
        }
      } while (FindNextFileA(hFind, &finder));
      FindClose(hFind);
    } // handle exists
#else
    std::string filename = cfgMap;
    if (tmp != std::string::npos) filename = filename.substr(tmp + 1);
    this->autoMapPool = {filename};
#endif

    mapChanged = true;
  } // if map was changed^

  // Get map iteration config
  std::string newMapIteration = LoadConfigStringUCase("auto_menu", "mapiteration", "RANDOM");
  if (this->autoMapIteration != newMapIteration)
  {
    this->autoMapIteration = newMapIteration;
    this->lastAutoMapEntry = 0;
    this->lastMapGen.clear();

    mapChanged = true;
  }

  if (mapChanged)
    this->chooseNewRandomMap();

  this->autoMenuLanMode = LoadConfigString("auto_menu", "lan_mode", "Local Area Network (UDP)");
  this->autoMenuRace = LoadConfigStringUCase("auto_menu", "race", "RANDOM");
  std::stringstream raceList(this->autoMenuRace);

  std::string currentrace = this->autoMenuRace.substr(0, this->autoMenuRace.find_first_of(','));

  for (int i = 0; i < (int)BWAPI::BroodwarImpl.getInstanceNumber() && raceList; ++i)
      std::getline(raceList, currentrace, ',');

  // trim whitespace outside quotations and then the quotations
  Util::trim(currentrace, Util::is_whitespace_or_newline);
  Util::trim(currentrace, [](char c) { return c == '"'; });

  this->autoMenuRace = currentrace;

  this->autoMenuEnemyRace[0] = LoadConfigStringUCase("auto_menu", "enemy_race", "RANDOM");
  for (unsigned int i = 1; i < 8; ++i)
  {
    std::string key = "enemy_race_" + std::to_string(i);
    this->autoMenuEnemyRace[i] = LoadConfigStringUCase("auto_menu", key.c_str(), "DEFAULT");
    if (this->autoMenuEnemyRace[i] == "DEFAULT")
      this->autoMenuEnemyRace[i] = this->autoMenuEnemyRace[0];
  }

  this->autoMenuEnemyCount = LoadConfigInt("auto_menu", "enemy_count", 1);
  this->autoMenuEnemyCount = std::min(std::max(this->autoMenuEnemyCount, 0U), 7U);

  this->autoMenuGameType = LoadConfigStringUCase("auto_menu", "game_type", "MELEE");
  this->autoMenuGameTypeExtra = LoadConfigString("auto_menu", "game_type_extra", "");
  this->autoMenuSaveReplay = LoadConfigString("auto_menu", "save_replay");

  this->autoMenuMinPlayerCount = LoadConfigInt("auto_menu", "wait_for_min_players", 2);
  this->autoMenuMaxPlayerCount = LoadConfigInt("auto_menu", "wait_for_max_players", 8);
  this->autoMenuWaitPlayerTime = LoadConfigInt("auto_menu", "wait_for_time", 30000);
}

void AutoMenuManager::chooseNewRandomMap()
{
  if (!this->autoMapPool.empty())
  {
    int chosenEntry = 0;
    if (this->autoMapIteration == "RANDOM")
    {
      // Obtain a random map file
      chosenEntry = rand() % this->autoMapPool.size();
    }
    else if (this->autoMapIteration == "SEQUENCE")
    {
      if (this->lastAutoMapEntry >= this->autoMapPool.size())
        this->lastAutoMapEntry = 0;
      chosenEntry = this->lastAutoMapEntry++;
    }
    std::string chosen = this->autoMapPool[chosenEntry];
    this->lastMapGen = this->autoMenuMapPath + chosen;
  }
}

unsigned int AutoMenuManager::getLobbyPlayerCount()
{
  unsigned int rval = 0;
  for (unsigned int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
  {
    if (bwgame.getPlayer(i).nType() == PlayerTypes::Player)
      ++rval;
  }
  return rval;
}
unsigned int AutoMenuManager::getLobbyPlayerReadyCount()
{
  unsigned int rval = 0;
  for (unsigned int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
  {
    if (bwgame.getPlayer(i).nType() == PlayerTypes::Player && bwgame.getPlayer(i).downloadStatus() >= 100)
      ++rval;
  }
  return rval;
}
unsigned int AutoMenuManager::getLobbyOpenCount()
{
  unsigned int rval = 0;
  for (unsigned int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
  {
    if (bwgame.getPlayer(i).nType() == PlayerTypes::EitherPreferHuman)
      ++rval;
  }
  return rval;
}

void AutoMenuManager::onMenuFrame()
{

  // Don't attempt auto-menu if we run into 50 error message boxes
  if (this->autoMapTryCount > 50)
    return;

  // Return if autoMenu is not enabled
  if (this->autoMenuMode == "" || this->autoMenuMode == "OFF")
    return;

#ifdef _DEBUG
  // Wait for a debugger if autoMenuPause is enabled, and in DEBUG
  if (this->autoMenuPause != "OFF")
    return;
#endif

}

void AutoMenuManager::startGame()
{
  bool isAutoSingle = this->autoMenuMode == "SINGLE_PLAYER";
  //bool isCreating = !this->autoMenuMapPath.empty();
  //bool isJoining = !this->autoMenuGameName.empty();
  
  std::string name = this->autoMenuCharacterName;
  if (name == "FIRST")
    name = "bwapi"; // this name will be used if there are no existing characters
  else if (name.empty())
    name = "empty";
  
  Broodwar->setCharacterName(name);
  
  //if (!Broodwar->setMap(this->lastMapGen)) throw std::runtime_error("AutoMenuManager::createGame: setMap failed :(");
  BWAPI::BroodwarImpl.bwgame.setMapFileName(this->lastMapGen);
  Broodwar->setGameType(GameType::getType(this->autoMenuGameType));
  
  if (isAutoSingle) {
    Broodwar->createSinglePlayerGame([&]() {
      if (Broodwar->isReplay())
      {
        Broodwar->startGame();
      }
      else
      {
        auto race = GameImpl::getMenuRace(this->autoMenuRace);
        if (Broodwar->self())
        {
          if (Broodwar->self()->getRace() != race)
            Broodwar->self()->setRace(race);
          unsigned enemyCount = 0;
          for (int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
          {
            Player p = Broodwar->getPlayer(i);
            if (p->getType() != PlayerTypes::EitherPreferHuman && p->getType() != PlayerTypes::EitherPreferComputer) continue;
            if (p == Broodwar->self()) continue;
            if (enemyCount < this->autoMenuEnemyCount)
              p->setRace(GameImpl::getMenuRace(this->autoMenuEnemyRace[enemyCount++]));
            else
              p->closeSlot();
          }
          Broodwar->startGame();
        }
        else
        {
          bool foundSlot = false;
          for (int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
          {
            Player p = Broodwar->getPlayer(i);
            if (p->getType() == PlayerTypes::EitherPreferHuman)
            {
              Broodwar->switchToPlayer(p);
              foundSlot = true;
              break;
            }
          }
          if (!foundSlot) throw std::runtime_error("createSinglePlayerGame: no available slot");
        }
      }
    });

  } else {
    
    Broodwar->createMultiPlayerGame([&]() {
      if (Broodwar->isReplay())
      {
        if (Broodwar->connectedPlayerCount() >= 2) Broodwar->startGame();
      }
      else
      {
        auto race = GameImpl::getMenuRace(this->autoMenuRace);
        if (Broodwar->self())
        {
          if (Broodwar->self()->getRace() != race)
            Broodwar->self()->setRace(race);
        }
        else {
          for (int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
          {
            Player p = Broodwar->getPlayer(i);
            if (p->getType() == PlayerTypes::EitherPreferHuman)
            {
              Broodwar->switchToPlayer(p);
              break;
            }
          }
        }
        int playerCount = 0;
        for (int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
        {
          Player p = Broodwar->getPlayer(i);
          if (p->getType() != PlayerTypes::Player && p->getType() != PlayerTypes::Computer) continue;
          ++playerCount;
        }
        if (playerCount >= 2) Broodwar->startGame();
      }
    });

  }

}
