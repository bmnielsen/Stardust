#include "GameImpl.h"
#include <time.h>

#include <Util/StringUtil.h>

#include <BW/BWData.h>

#include <BWAPI/PlayerImpl.h>
#include <BWAPI/RegionImpl.h>

#include "../../../svnrev.h"
#include "../../../Debug.h"

#include <chrono>
#include <codecvt>

#ifdef _WIN32
#include <windows.h>
#undef min
#undef max
#else
#include <sys/types.h>
#include <sys/stat.h>
#endif

namespace BWAPI
{
  //--------------------------------------------- ON GAME START ----------------------------------------------
  void GameImpl::onGameStart()
  {
    // This function is called at the start of every match
    this->initializeData();

    // initialize the variables
    //this->frameCount      = 0;
    this->onStartCalled   = true;
    this->calledMatchEnd  = false;

    // pre-calculate the map hash
    map.calculateMapHash();

    // Obtain Broodwar Regions
    size_t rgnCount = bwgame.regionCount();
    // Iterate regions and insert into region list
    for (size_t i = 0; i < rgnCount; ++i)
    {
      Region r = new BWAPI::RegionImpl(i);
      this->regionsList.insert(r);
      this->regionMap[i] = r;
    }

    // Iterate regions again and update neighbor lists
    for ( BWAPI::Region r : this->regionsList )
      static_cast<RegionImpl*>(r)->UpdateRegionRelations();

    // roughly identify which players can possibly participate in this game
    for (int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
    {
      // reset participation and resources
      if (this->players[i])
      {
        this->players[i]->setParticipating(false);
        this->players[i]->resetResources();
      }

      // First check if player owns a unit at start
      for (int u = 0; u < UnitTypes::None; ++u)
      {
        if (bwgame.getPlayer(i).unitCountsAll(u))
        {
          if (this->players[i])
            this->players[i]->setParticipating();
          break;
        }
      }
    }

    for (int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
    {
      if ( !this->players[i] || !this->players[i]->isObserver() )
        continue;
      if (bwgame.triggersCanAllowGameplayForPlayer(i))
        this->players[i]->setParticipating();
    }

    if (bwgame.InReplay()) // set replay flags
    {
      // Set every cheat flag to true
      for (int i = 0; i < Flag::Max; ++i)
        this->flags[i] = true;
    }
    else
    {
      // Get the current player
      BWAPI::PlayerImpl *thisPlayer = this->_getPlayer(_currentPlayerId());
      if ( !thisPlayer )
        return;

      this->BWAPIPlayer = thisPlayer;

      // find the opponent player
      for (int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
      {
        if ( (this->players[i]->getType() == PlayerTypes::Computer ||
              this->players[i]->getType() == PlayerTypes::Player   ||
              this->players[i]->getType() == PlayerTypes::EitherPreferComputer) &&
             !this->players[i]->isObserver() &&
             thisPlayer->isEnemy(this->players[i]) )
          this->enemyPlayer = this->players[i];
      }
    }

    // get pre-race info
    std::array<int, 12> bRaceInfo = bwgame.bRaceInfo();
    std::array<int, 12> bOwnerInfo = bwgame.bOwnerInfo();

    for (int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
    {
      // Skip Start Locations that don't exist
      if (bwgame.getPlayer(i).startPosition() == Positions::Origin)
        continue;

      // If the game is UMS and player is observer and race is not (UserSelect OR invalid player type), skip
      if ( this->getGameType() == GameTypes::Use_Map_Settings &&
           this->players[i]->isObserver() &&
           (bRaceInfo[i] != Races::Enum::Select ||
           (bOwnerInfo[i] != PlayerTypes::Computer &&
            bOwnerInfo[i] != PlayerTypes::Player   &&
            bOwnerInfo[i] != PlayerTypes::EitherPreferComputer &&
            bOwnerInfo[i] != PlayerTypes::EitherPreferHuman)) )
        continue;

      // add start location
      startLocations.push_back(TilePosition(bwgame.getPlayer(i).startPosition() - Position(64, 48)));
    }

    // Get Player Objects
    for (int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
    {
      if ( this->players[i] &&
           bwgame.getPlayer(i).nType() != PlayerTypes::None &&
           bwgame.getPlayer(i).nType() <  PlayerTypes::Closed )
      {
        players[i]->setID(server.getPlayerID(players[i]));
        this->playerSet.insert(this->players[i]);
      }
    }

    if ( this->BWAPIPlayer )
    {
      for (Player p : players)
      {
        if ( p->leftGame() || p->isDefeated() || p == BWAPIPlayer )
          continue;
        if ( BWAPIPlayer->isAlly(p) )
          _allies.insert(p);
        if ( BWAPIPlayer->isEnemy(p) )
          _enemies.insert(p);
        if ( p->isObserver() )
          _observers.insert(p);
      }
    }

    if ( this->players[11] )
      this->playerSet.insert(this->players[11]);

    for (int p = 0; p < BW::PLAYABLE_PLAYER_COUNT; ++p) {
        if (this->players[p]) this->players[p]->force = nullptr;
    }

    // Get Force Objects, assign Force<->Player relations
    ForceImpl *pNeutralForce = new ForceImpl("");
    if (this->players[11])
    {
      pNeutralForce->players.insert(this->players[11]);
      this->players[11]->force = pNeutralForce;
    }

    for ( int f = 1; f <= 4; ++f )
    {
      ForceImpl *newForce = new ForceImpl( bwgame.forceNames(f-1) );
      this->forces.insert( newForce );
      for (int p = 0; p < BW::PLAYABLE_PLAYER_COUNT; ++p)
      {
        if ( this->players[p] && bwgame.getPlayer(p).nTeam() == f )
        {
          this->players[p]->force = newForce;
          if ( bwgame.getPlayer(p).nType() != PlayerTypes::None &&
               bwgame.getPlayer(p).nType() <  PlayerTypes::Closed )
            newForce->players.insert(this->players[p]);
        }
      }
    }

    // Assign neutral force to players that do not have one
    for (int p = 0; p < BW::PLAYABLE_PLAYER_COUNT; ++p)
    {
      if ( this->players[p] && !this->players[p]->force )
        this->players[p]->force = pNeutralForce;
    }

    // Get info for replay naming
    rn_GameResult = "loss"; // Game is counted as lost by default

    if ( !this->isReplay() )
    {
      auto utf8Substr = [&](const std::string& str, size_t pos, size_t count) {
        std::string r;
        size_t i = 0;
        size_t p = 0;
        auto next = [&]() {
          unsigned char mask = str[i] & 0xf0;
          ++p;
          if (mask == 0xf0) i += 4;
          else if (mask == 0xe0) i += 3;
          else if (mask == 0xc0) i += 2;
          else ++i;
        };
        while (p != pos && i < str.size()) next();
        while (p != pos + count && i < str.size()) {
          size_t li = i;
          next();
          if (i > str.size()) i = str.size();
          r.append(str, li, i - li);
        }
        return r;
      };

      if ( BWAPIPlayer )
      {
        rn_BWAPIName = BWAPIPlayer->getName();
        rn_BWAPIRace = BWAPIPlayer->getRace().getName().substr(0, 1);
      }
      rn_MapName   = utf8Substr(mapName(), 0, 16);
      rn_AlliesNames.clear();
      rn_AlliesRaces.clear();
      rn_EnemiesNames.clear();
      rn_EnemiesRaces.clear();
      for ( Player p : this->_allies )
      {
        if ( p )
        {
          rn_AlliesNames += utf8Substr(p->getName(), 0, 6);
          rn_AlliesRaces += p->getRace().getName().substr(0, 1);
        }
      }
      for ( Player p : this->_enemies )
      {
        if ( p )
        {
          rn_EnemiesNames += p->getType() != PlayerTypes::Computer ? utf8Substr(p->getName(), 0, 6) : "Comp";
          rn_EnemiesRaces += p->getRace().getName().substr(0, 1);
        }
      }
    } // !isReplay
  }
  //--------------------------------------------------- ON SAVE ------------------------------------------------
  void GameImpl::onSaveGame(const char *name)
  {
    // called when the game is being saved
    events.push_back(Event::SaveGame(name));
  }
  //---------------------------------------------- ON SEND TEXT ----------------------------------------------
  void GameImpl::onSendText(const std::string &text)
  {
    if ( text.empty() ) return;

    if ( !parseText(text) && isFlagEnabled(BWAPI::Flag::UserInput) )
    {
      if ( externalModuleConnected )
      {
        events.push_back(Event::SendText());
        events.back().setText(text.c_str());
      }
      else
        sendText("%s", text.c_str());
    }
  }
  //---------------------------------------------- ON RECV TEXT ----------------------------------------------
  void GameImpl::onReceiveText(int playerId, const std::string &text)
  {
    if ( !this->bTournamentMessageAppeared && hTournamentModule && text == getTournamentString() )
      this->bTournamentMessageAppeared = true;

    // Do onReceiveText
    int realId = stormIdToPlayerId(playerId);
    if ( realId != -1 &&
         (!this->BWAPIPlayer ||
          realId != this->BWAPIPlayer->getIndex() ) &&
         this->isFlagEnabled(BWAPI::Flag::UserInput) )
    {
      events.push_back(Event::ReceiveText(this->players[realId]));
      events.back().setText(text.c_str());
    }
  }
  int fixPathString(const char *in, char *out_, size_t outLen)
  {
    unsigned int n = 0;
    const unsigned char *_in = (const unsigned char*)in;
    for ( unsigned int i = 0; _in[i] != 0 && n < outLen-1; ++i )
    {
      if ( !iscntrl(_in[i]) &&
           _in[i] != '?'    &&
           _in[i] != '*'    &&
           _in[i] != '<'    &&
           _in[i] != '|'    &&
           _in[i] != '"'    &&
           _in[i] != ':' )
      {
        if ( _in[i] == '/' )
          out_[n] = '\\';
        else
          out_[n] = _in[i];
        ++n;
      }
    }
    out_[n] = 0;
    return n;
  }

  void ignore_invalid_parameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
  {}
  //---------------------------------------------- ON GAME END -----------------------------------------------
  void GameImpl::onGameEnd()
  {
    //this is called at the end of every match
    if ( !this->onStartCalled )
      return;
    
    if ( autoMenuManager.autoMenuSaveReplay != "" && !this->isReplay() )
    {
      std::string pathStr = autoMenuManager.autoMenuSaveReplay;
      
      auto replace = [&](std::string name, std::string value) {
        name = '%' + name + '%';
        for (size_t i = 0;;) {
          i = pathStr.find(name, i);
          if (i == std::string::npos) break;
          pathStr.replace(i, name.size(), value);
          ++i;
        }
      };
      
      replace("BOTNAME",    rn_BWAPIName.c_str());
      replace("BOTNAME6",   rn_BWAPIName.substr(0,6).c_str());
      replace("BOTRACE",    rn_BWAPIRace.c_str());
      replace("MAP",        rn_MapName.c_str());
      replace("ALLYNAMES",  rn_AlliesNames.c_str());
      replace("ALLYRACES",  rn_AlliesRaces.c_str());
      replace("ENEMYNAMES", rn_EnemiesNames.c_str());
      replace("ENEMYRACES", rn_EnemiesRaces.c_str());
      replace("GAMERESULT", rn_GameResult.c_str ());

      // Double any %'s remaining in the string so that strftime executes correctly
      {
        size_t tmp = std::string::npos;
        while (tmp = pathStr.find_last_of('%', tmp - 1), tmp != std::string::npos)
          pathStr.insert(tmp, "%");
      }

      // Replace the placeholder $'s with %'s for the strftime call
      std::replace(pathStr.begin(), pathStr.end(), '$', '%');

      // Get time
      time_t tmpTime = std::time(nullptr);
      tm timeInfo;
#ifdef _MSC_VER
      localtime_s(&timeInfo, &tmpTime);
#else
      localtime_r(&tmpTime, &timeInfo);
#endif

      // Expand time strings, add a handler for this specific task to ignore errors in the format string
      char buf[0x100];
      std::strftime(buf, sizeof(buf), pathStr.c_str(), &timeInfo);
      pathStr = buf;

      std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
      // Remove illegal characters
      pathStr.erase(std::remove_if(pathStr.begin(), pathStr.end(),
                                   [](char c) {
                                     if ((unsigned char)c >= 128) return false;
                                     if (c >= 'a' && c <= 'z') return false;
                                     if (c >= 'A' && c <= 'Z') return false;
                                     if (c >= '0' && c <= '9') return false;
                                     if (c == '-' || c == '-') return false;
                                     if (c == '_' || c == '_') return false;
                                     if (c == '.') return false;
                                     if (c == '/') return false;
                                     if (c == ' ') return false;
                                     return true;
                                   }), pathStr.end());

      for (size_t i = 0; i != pathStr.size(); ++i) {
        if (pathStr[i] == '/') {
          std::string n(pathStr.c_str(), i);
#ifdef _WIN32
          std::wstring u16_n= std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t>{}.from_bytes(n);
		  CreateDirectoryW(u16_n.c_str(), nullptr);
#else
          mkdir(n.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
#endif
        }
      }
      
      bwgame.saveReplay(pathStr);
    }

    if ( !this->calledMatchEnd )
    {
      this->calledMatchEnd = true;
      events.push_back(Event::MatchFrame());
      events.push_back(Event::MatchEnd(false));
      server.update();
      this->inGame = false;
      events.push_back(Event::MenuFrame());
      server.update();
    }

    // player-specific game end
    for (int i = 0; i < BW::PLAYER_COUNT; ++i)
      if ( this->players[i] )
        this->players[i]->onGameEnd();

    this->onStartCalled = false;

    this->initializeData();
    this->autoMenuManager.chooseNewRandomMap();
  }
  //---------------------------------------------- SEND EVENTS TO CLIENT
  void GameImpl::SendClientEvent(BWAPI::AIModule *module, Event &e)
  {
    EventType::Enum et = e.getType();
    switch (et)
    {
    case EventType::MatchStart:
      module->onStart();
      break;
    case EventType::MatchEnd:
      module->onEnd(e.isWinner());
      break;
    case EventType::MatchFrame:
      module->onFrame();
      break;
    case EventType::MenuFrame:
      break;
    case EventType::SendText:
      module->onSendText(e.getText());
      break;
    case EventType::ReceiveText:
      module->onReceiveText(e.getPlayer(), e.getText());
      break;
    case EventType::PlayerLeft:
      module->onPlayerLeft(e.getPlayer());
      break;
    case EventType::NukeDetect:
      module->onNukeDetect(e.getPosition());
      break;
    case EventType::UnitDiscover:
      module->onUnitDiscover(e.getUnit());
      break;
    case EventType::UnitEvade:
      module->onUnitEvade(e.getUnit());
      break;
    case EventType::UnitCreate:
      module->onUnitCreate(e.getUnit());
      break;
    case EventType::UnitDestroy:
      module->onUnitDestroy(e.getUnit());
      break;
    case EventType::UnitMorph:
      module->onUnitMorph(e.getUnit());
      break;
    case EventType::UnitShow:
      module->onUnitShow(e.getUnit());
      break;
    case EventType::UnitHide:
      module->onUnitHide(e.getUnit());
      break;
    case EventType::UnitRenegade:
      module->onUnitRenegade(e.getUnit());
      break;
    case EventType::SaveGame:
      module->onSaveGame(e.getText());
      break;
    case EventType::UnitComplete:
      module->onUnitComplete(e.getUnit());
      break;
    default:
      break;
    }
  }
  //---------------------------------------------- PROCESS EVENTS
  void GameImpl::processEvents()
  {
    //This function translates events into AIModule callbacks
    if ( !client || server.isConnected() )
      return;
    auto eventsCopy = events;
    for (Event e : eventsCopy)
    {
      std::chrono::high_resolution_clock::time_point startTime;

      // Reset event stopwatch
      if ( tournamentAI )
      {
        this->lastEventTime = 0;
        startTime = std::chrono::high_resolution_clock::now();
      }

      // Send event to the AI Client module
      SendClientEvent(client, e);

      // continue if the tournament is not loaded
      if ( !tournamentAI )
        continue;

      // Save the last event time
       this->lastEventTime = (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime).count();

      // Send same event to the Tournament module for post-processing
      isTournamentCall = true;
      SendClientEvent(tournamentAI, e);
      isTournamentCall = false;

      if (events.empty()) break;
    } // foreach event
  }
}

