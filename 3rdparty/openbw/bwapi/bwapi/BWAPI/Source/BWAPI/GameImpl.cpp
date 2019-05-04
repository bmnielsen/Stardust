#include "../../../svnrev.h"
#include "GameImpl.h"

#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

#include <Util/Clamp.h>
#include <Util/Convenience.h>

#include <BWAPI/ForceImpl.h>
#include <BWAPI/PlayerImpl.h>
#include <BWAPI/UnitImpl.h>
#include <BWAPI/BulletImpl.h>
#include <Templates.h>
#include <BWAPI/Command.h>
#include <BWAPI/Map.h>
#include <BWAPI/Flag.h>
#include <BWAPI/UnaryFilter.h>

#include <BWAPI/Unitset.h>

#include <BW/CheatType.h>
#include <BW/OrderTypes.h>

#include "BWAPI/AIModule.h"
#include "../Config.h"
#include "BWtoBWAPI.h"

#include "../../../Debug.h"

namespace BWAPI
{
  //----------------------------------------------------------------------------------------------------------
  Force GameImpl::getForce(int forceID) const
  {
    return server.getForce(forceID);
  }
  //----------------------------------------------------------------------------------------------------------
  Region GameImpl::getRegion(int regionID) const
  {
    auto it = regionMap.find(regionID);
    return it != regionMap.end() ? it->second : nullptr;
  }
  //----------------------------------------------------------------------------------------------------------
  Player GameImpl::getPlayer(int playerID) const
  {
    if (!isInGame()) return (size_t)playerID < BW::PLAYER_COUNT ? players[playerID] : nullptr;
    return server.getPlayer(playerID);
  }
  //----------------------------------------------------------------------------------------------------------
  Unit GameImpl::getUnit(int unitID) const
  {
    return server.getUnit(unitID);
  }
  //----------------------------------------------------------------------------------------------------------
  Unit GameImpl::indexToUnit(int unitIndex) const
  {
    if ( !this->isFlagEnabled(Flag::CompleteMapInformation) )
      return nullptr;
    int i = (unitIndex & 0x7FF);
    if ((size_t)i < this->unitArray.size() && this->unitArray[i]->canAccess())
      return this->unitArray[i];
    return nullptr;
  }
  //--------------------------------------------- GET GAME TYPE ----------------------------------------------
  GameType GameImpl::getGameType() const
  {
    if (isReplay())
      return GameTypes::None;
    return GameType(bwgame.gameType());
  }
  //---------------------------------------------- GET LATENCY -----------------------------------------------
  int GameImpl::getLatency() const
  {
    // Returns the real latency values
    if ( !this->isMultiplayer() )
      return BWAPI::Latency::SinglePlayer;

    if ( this->isBattleNet() )
    {
      switch(bwgame.Latency())
      {
        case 0:
          return BWAPI::Latency::BattlenetLow;
        case 1:
          return BWAPI::Latency::BattlenetMedium;
        case 2:
          return BWAPI::Latency::BattlenetHigh;
        default:
          return BWAPI::Latency::BattlenetLow;
      }
    }
    else
    {
      switch(bwgame.Latency())
      {
        case 0:
          return BWAPI::Latency::LanLow;
        case 1:
          return BWAPI::Latency::LanMedium;
        case 2:
          return BWAPI::Latency::LanHigh;
        default:
          return BWAPI::Latency::LanLow;
      }
    }
  }
  //--------------------------------------------- GET FRAME COUNT --------------------------------------------
  int GameImpl::getFrameCount() const
  {
    return this->frameCount;
  }
  //--------------------------------------------- GET REPLAY FRAME COUNT -------------------------------------
  int GameImpl::getReplayFrameCount() const
  {
    return bwgame.ReplayHead_frameCount();
  }
  //------------------------------------------------ GET FPS -------------------------------------------------
  int GameImpl::getFPS() const
  {
    return fpsCounter.getFps();
  }
  //-------------------------------------------- GET Average FPS ---------------------------------------------
  double GameImpl::getAverageFPS() const
  {
    return fpsCounter.getAverageFps();
  }
  //------------------------------------------- GET MOUSE POSITION -------------------------------------------
  BWAPI::Position GameImpl::getMousePosition() const
  {
    if ( !this->isFlagEnabled(BWAPI::Flag::UserInput) )
      return BWAPI::Positions::Unknown;
    return BWAPI::Position(bwgame.MouseX(), bwgame.MouseY());
  }
  //--------------------------------------------- GET MOUSE STATE --------------------------------------------
  bool GameImpl::getMouseState(MouseButton button) const
  {
    if ( !this->isFlagEnabled(BWAPI::Flag::UserInput) )
      return false;

    // fixme
    return false;
//    int vkValue;
//    switch ( button )
//    {
//      case BWAPI::M_LEFT:
//        vkValue = VK_LBUTTON;
//        break;
//      case BWAPI::M_RIGHT:
//        vkValue = VK_RBUTTON;
//        break;
//      case BWAPI::M_MIDDLE:
//        vkValue = VK_MBUTTON;
//        break;
//      default:
//        return false;
//    }
//    return (GetKeyState(vkValue) & 0x80) > 0;
  }
  //---------------------------------------------- GET KEY STATE ---------------------------------------------
  bool GameImpl::getKeyState(Key key) const
  {
    if ( !this->isFlagEnabled(BWAPI::Flag::UserInput) )
      return false;

    if ( key < 0 || key >= K_MAX )
      return false;

    // fixme
    return false;
    //return (GetKeyState(key) & 128) > 0;
  }
  //------------------------------------------- GET SCREEN POSITION ------------------------------------------
  BWAPI::Position GameImpl::getScreenPosition() const
  {
    if ( !this->isFlagEnabled(BWAPI::Flag::UserInput) )
      return BWAPI::Positions::Unknown;
    return BWAPI::Position(bwgame.ScreenX(), bwgame.ScreenY());
  }
  //------------------------------------------- SET SCREEN POSITION ------------------------------------------
  void GameImpl::setScreenPosition(int x, int y)
  {
    this->setLastError();
    if ( !data->hasGUI ) return;

    bwgame.setScreenPosition(x, y);
//    Position scrSize(bwgame.GameScreenBuffer_width(), bwgame.GameScreenBuffer_height()-108);
//    Position mapSize( TilePosition(map.getWidth(), map.getHeight()) );

//    // Sets the screen's position relative to the map
//    Position movePos(x,y);
//    movePos.setMin(0, 0);
//    movePos.setMax( mapSize - scrSize);

//    movePos &= 0xFFFFFFF8;
//    bwgame.MoveToX(movePos.x);
//    bwgame.MoveToY(movePos.y);
//    bwgame.Game_screenTilePosition() = BW::TilePosition(movePos);
//    bwgame.BWFXN_UpdateScreenPosition();
  }
  //---------------------------------------------- PING MINIMAP ----------------------------------------------
  void GameImpl::pingMinimap(int x, int y)
  {
    this->setLastError();
    bwgame.QueueCommand<BW::Orders::MinimapPing>(x, y);
  }
  //--------------------------------------------- IS FLAG ENABLED --------------------------------------------
  bool GameImpl::isFlagEnabled(int flag) const
  {
    // Check if index is valid
    if ( flag < 0 || flag >= BWAPI::Flag::Max ) 
      return false;

    // Make completeMapInfo appear true if the match has ended
    if ( flag == Flag::CompleteMapInformation && this->calledMatchEnd )
      return true;

    // Return the state of the flag
    return this->flags[flag];
  }
  //----------------------------------------------- ENABLE FLAG ----------------------------------------------
  void  GameImpl::enableFlag(int flag)
  {
    // Enable the specified flag
    this->setLastError();

    // Don't allow flag changes after frame 0
    if ( this->frameCount > 0 )
    {
      this->setLastError(Errors::Access_Denied);
      return;
    }

    // check if index is valid
    if ( flag < 0 || flag >= BWAPI::Flag::Max )
    {
      this->setLastError(Errors::Invalid_Parameter);
      return;
    }
    
    // check if tournament will allow the call
    if ( !this->tournamentCheck(Tournament::EnableFlag, &flag) )
      return;

    // Modify flag state 
    this->flags[flag] = true;
    if ( !this->hTournamentModule )
    {
      switch(flag)
      {
      case BWAPI::Flag::CompleteMapInformation:
        this->sendText("Enabled Flag CompleteMapInformation");
        break;
      case BWAPI::Flag::UserInput:
        this->sendText("Enabled Flag UserInput");
        break;
      }
    }
  }
  //--------------------------------------------- GET UNITS IN RECTANGLE -------------------------------------
  Unit GameImpl::_unitFromIndex(int index)
  {
    return this->unitArray[index-1];
  }
  Unitset GameImpl::getUnitsInRectangle(int left, int top, int right, int bottom, const UnitFilter &pred) const
  {
    Unitset unitFinderResults;

    // Have the unit finder do its stuff
    Templates::iterateUnitFinder(bwgame.UnitOrderingX(),
                                                 bwgame.UnitOrderingY(),
                                                 bwgame.UnitOrderingCount(),
                                                 left,
                                                 top,
                                                 right,
                                                 bottom,
                                                 [&](Unit u){ if ( !pred.isValid() || pred(u) )
                                                                 unitFinderResults.insert(u); });
    // Return results
    return unitFinderResults;
  }
  Unit GameImpl::getClosestUnitInRectangle(Position center, const UnitFilter &pred, int left, int top, int right, int bottom) const
  {
    // cppcheck-suppress variableScope
    int bestDistance = 99999999;
    Unit pBestUnit = nullptr;

    Templates::iterateUnitFinder( bwgame.UnitOrderingX(),
                                  bwgame.UnitOrderingY(),
                                  bwgame.UnitOrderingCount(),
                                  left,
                                  top,
                                  right,
                                  bottom,
                                  [&](Unit u){ if ( !pred.isValid() || pred(u) )
                                                {
                                                  int newDistance = u->getDistance(center);
                                                  if ( newDistance < bestDistance )
                                                  {
                                                    pBestUnit = u;
                                                    bestDistance = newDistance;
                                                  }
                                                } } );
    return pBestUnit;
  }
  Unit GameImpl::getBestUnit(const BestUnitFilter &best, const UnitFilter &pred, Position center, int radius) const
  {
    Unit pBestUnit = nullptr;
    Position rad(radius,radius);
    
    Position topLeft(center - rad);
    Position botRight(center + rad);

    topLeft.makeValid();
    botRight.makeValid();

    Templates::iterateUnitFinder( bwgame.UnitOrderingX(),
                                  bwgame.UnitOrderingY(),
                                  bwgame.UnitOrderingCount(),
                                  topLeft.x,
                                  topLeft.y,
                                  botRight.x,
                                  botRight.y,
                                  [&](Unit u){ if ( !pred.isValid() || pred(u) )
                                                {
                                                  if ( pBestUnit == nullptr )
                                                    pBestUnit = u;
                                                  else
                                                    pBestUnit = best(pBestUnit,u); 
                                                } } );

    return pBestUnit;
  }
  //----------------------------------------------- MAP WIDTH ------------------------------------------------
  int GameImpl::mapWidth() const
  {
    return map.getWidth();
  }
  //----------------------------------------------- MAP HEIGHT -----------------------------------------------
  int GameImpl::mapHeight() const
  {
    return map.getHeight();
  }
  //---------------------------------------------- MAP FILE NAME ---------------------------------------------
  std::string GameImpl::mapFileName() const
  {
    return map.getFileName();
  }
  //---------------------------------------------- MAP PATH NAME ---------------------------------------------
  std::string GameImpl::mapPathName() const
  {
    return map.getPathName();
  }
  //------------------------------------------------ MAP NAME ------------------------------------------------
  std::string GameImpl::mapName() const
  {
    return map.getName();
  }
  //----------------------------------------------- GET MAP HASH ---------------------------------------------
  std::string GameImpl::mapHash() const
  {
    return map.getMapHash();
  }
  //--------------------------------------------- IS WALKABLE ------------------------------------------------
  bool GameImpl::isWalkable(int x, int y) const
  {
    return map.walkable(x, y);
  }
  //--------------------------------------------- GET GROUND HEIGHT ------------------------------------------
  int GameImpl::getGroundHeight(int x, int y) const
  {
    return map.groundHeight(x, y);
  }
  //--------------------------------------------- IS BUILDABLE -----------------------------------------------
  bool GameImpl::isBuildable(int x, int y, bool includeBuildings) const
  {
    if ( map.buildable(x,y) )
    {
      if ( includeBuildings && this->isVisible(x,y) && map.isOccupied(x,y) )
        return false;
      return true;
    }
    return false;
  }
  //--------------------------------------------- IS VISIBLE -------------------------------------------------
  bool GameImpl::isVisible(int x, int y) const
  {
    return map.visible(x, y);
  }
  //--------------------------------------------- IS EXPLORED ------------------------------------------------
  bool GameImpl::isExplored(int x, int y) const
  {
    return map.isExplored(x, y);
  }
  //--------------------------------------------- HAS CREEP --------------------------------------------------
  bool GameImpl::hasCreep(int x, int y) const
  {
    if (!this->isFlagEnabled(Flag::CompleteMapInformation) && !this->isVisible(x, y))
      return false;
    return map.hasCreep(x, y);
  }
  //--------------------------------------------- HAS POWER --------------------------------------------------
  bool GameImpl::hasPowerPrecise(int x, int y, UnitType unitType) const
  {
    return Templates::hasPower(x, y, unitType, pylons);
  }
  //------------------------------------------------- PRINTF -------------------------------------------------
  void GameImpl::vPrintf(const char *format, va_list arg)
  {
    // nogui & safety
    //if ( !data->hasGUI ) return;
    if ( !format ) return;
    
    // Expand format into buffer
    char buffer[512];
    VSNPrintf(buffer, format, arg);

    if ( !this->tournamentCheck(Tournament::Printf, buffer) )
      return;
    
    bwgame.printText(buffer);

    // Dispatch message using existing Storm library function (lobby+game)
//    S_EVT evt = { 4, -1, buffer, strlen(buffer) + 1 };
//    SEvtDispatch('SNET', 1, 4, &evt);
    // fixme
  }
  //--------------------------------------------- SEND TEXT --------------------------------------------------
  void GameImpl::vSendTextEx(bool toAllies, const char *format, va_list arg)
  {
    // safety
    if ( !format ) return;
    
    // Expand format and store in buffer
    char buffer[80]; // Use maximum size of 80 since there is a hardcoded limit in Broodwar of 80 characters
    VSNPrintf(buffer, format, arg);

    // Check if tournament module allows sending text
    if ( !this->tournamentCheck(Tournament::SendText, buffer) )
      return;

    if ( buffer[0] == '/' )    // If we expect a battle.net command
    {
      //SNetSendServerChatCommand(buffer);
      // fixme
      return;
    }

    if ( this->isReplay() )  // Just print the text if in a replay
    {
      Broodwar << buffer << std::endl;
      return;
    }

    // If we're in single player
    if ( this->isInGame() && !this->isMultiplayer() )
    {
      BW::CheatFlags::Enum cheatID = BW::getCheatFlag(buffer);
      if ( cheatID != BW::CheatFlags::None )  // Apply cheat code if it is one
      {
        this->cheatFlags ^= cheatID;
        bwgame.QueueCommand<BW::Orders::UseCheat>(this->cheatFlags);
        if (cheatID == BW::CheatFlags::ShowMeTheMoney ||
            cheatID == BW::CheatFlags::BreatheDeep ||
            cheatID == BW::CheatFlags::WhatsMineIsMine ||
            cheatID == BW::CheatFlags::SomethingForNothing)
          this->cheatFlags ^= cheatID;
      }
      else  // Just print the message otherwise
      {
        Broodwar << this->BWAPIPlayer->getTextColor() << this->BWAPIPlayer->getName()
                  << ": " << Text::Green << buffer << std::endl;
      }
    } // single
    else  // multiplayer or lobby
    {
      // fixme
//      // Otherwise, send the message using Storm command
//      char szMessage[82];
//      szMessage[0] = 0;
//      szMessage[1] = 1;
//      int msgLen = SStrCopy(&szMessage[2], buffer, 80);

//      if ( this->isInGame() )    // in game
//      {
//        if ( toAllies ) // Send to all allies
//        {
//          for (int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
//            if ( this->BWAPIPlayer->isAlly(players[i]) && BW::BWDATA::Players[i].dwStormId != -1 )
//              SNetSendMessage(BW::BWDATA::Players[i].dwStormId, szMessage, msgLen + 3);
//        }
//        else  // Otherwise send to all
//        {
//          SNetSendMessage(-1, szMessage, msgLen + 3);
//        } // toAllies
//      }
//      else  // assume in lobby, then send lobby message
//      {
//        szMessage[1] = 0x4C;
//        SNetSendMessage(-1, &szMessage[1], msgLen + 2);
//      } // isInGame
    } // multi
  }
  //------------------------------------------------ IS IN GAME ----------------------------------------------
  bool GameImpl::isInGame() const
  {
    return inGame;
  }
  //-------------------------------------------- IS SINGLE PLAYER --------------------------------------------
  bool GameImpl::isMultiplayer() const
  {
    return bwgame.NetMode() != 0 && bwgame.NetMode() != -1;
  }
  //--------------------------------------------- IS BATTLE NET ----------------------------------------------
  bool GameImpl::isBattleNet() const
  {
    return bwgame.NetMode() == 0x424e4554; // "BNET"
  }
  //----------------------------------------------- IS PAUSED ------------------------------------------------
  bool GameImpl::isPaused() const
  {
    return bwgame.isGamePaused() != 0;
  }
  //----------------------------------------------- IN REPLAY ------------------------------------------------
  bool  GameImpl::isReplay() const
  {
    return bwgame.InReplay() != 0;
  }
  //----------------------------------------------- START GAME -----------------------------------------------
  void GameImpl::_startGame()
  {
    throw std::runtime_error("_startGame fixme");
    // Starts the game as a lobby host 
    //bwgame.QueueCommand<BW::Orders::StartGame>();
  }
  //----------------------------------------------- PAUSE GAME -----------------------------------------------
  void GameImpl::pauseGame()
  {
    // Pauses the game 
    this->setLastError();
    if ( !this->tournamentCheck(Tournament::PauseGame) )
      return;
    bwgame.QueueCommand<BW::Orders::PauseGame>();
  }
  //---------------------------------------------- RESUME GAME -----------------------------------------------
  void GameImpl::resumeGame()
  {
    // Resumes the game 
    this->setLastError();
    if ( !this->tournamentCheck(Tournament::ResumeGame) )
      return;
    bwgame.QueueCommand<BW::Orders::ResumeGame>();
  }
  //---------------------------------------------- LEAVE GAME ------------------------------------------------
  void GameImpl::leaveGame()
  {
    // Leaves the current game. Moves directly to the post-game score screen 
    this->setLastError();
    if ( !this->tournamentCheck(Tournament::LeaveGame) )
      return;
    bwgame.leaveGame();
  }
  //--------------------------------------------- RESTART GAME -----------------------------------------------
  void GameImpl::restartGame()
  {
    // Restarts the current match 
    // Does not work on Battle.net
    if ( this->isMultiplayer() )
    {
      this->setLastError(Errors::Invalid_Parameter);
      return;
    }

    this->setLastError();
    bwgame.QueueCommand<BW::Orders::RestartGame>();
  }
  //--------------------------------------------- SET ALLIANCE -----------------------------------------------
  bool GameImpl::setAlliance(BWAPI::Player player, bool allied, bool alliedVictory)
  {
    // Set the current player's alliance status 
    if ( !BWAPIPlayer || isReplay() || !player || player == BWAPIPlayer )
      return this->setLastError(Errors::Invalid_Parameter);

    u32 alliance = 0;
    for (int i = 0; i < BW::PLAYER_COUNT; ++i)
      alliance |= (bwgame.getPlayer(BWAPIPlayer->getIndex()).playerAlliances(i) & 3) << (i*2);
    
    u8 newAlliance = allied ? (alliedVictory ? 2 : 1) : 0;
    if ( allied )
      alliance |= newAlliance << ( static_cast<PlayerImpl*>(player)->getIndex()*2);
    else
      alliance &= ~(3 << ( static_cast<PlayerImpl*>(player)->getIndex()*2) );
    bwgame.QueueCommand<BW::Orders::SetAllies>(alliance);
    return this->setLastError();
  }
  //----------------------------------------------- SET VISION -----------------------------------------------
  bool GameImpl::setVision(BWAPI::Player player, bool enabled)
  {
    if ( !player )  // Parameter check
      return this->setLastError(Errors::Invalid_Parameter);

    if ( this->isReplay() )
    {
      u32 vision = bwgame.ReplayVision();
      if ( enabled )
        vision |= 1 << static_cast<PlayerImpl*>(player)->getIndex();
      else
        vision &= ~(1 <<  static_cast<PlayerImpl*>(player)->getIndex() );
      bwgame.setReplayVision(vision);
    }
    else
    {
      if ( !BWAPIPlayer || player == BWAPIPlayer )
        return this->setLastError(Errors::Invalid_Parameter);

      u16 vision = static_cast<u16>(bwgame.getPlayer(BWAPIPlayer->getIndex()).playerVision());
      if ( enabled )
        vision |= 1 << static_cast<PlayerImpl*>(player)->getIndex();
      else
        vision &= ~(1 <<  static_cast<PlayerImpl*>(player)->getIndex() );
      bwgame.QueueCommand<BW::Orders::SetVision>(vision);
    }
    return this->setLastError();
  }
  //--------------------------------------------------- GAME SPEED -------------------------------------------
  void GameImpl::setLocalSpeed(int speed)
  {
    // Sets the frame rate of the client 
    if (!this->tournamentCheck(Tournament::SetLocalSpeed, &speed) ||
      this->speedOverride != std::numeric_limits<decltype(this->speedOverride)>::min()) return;

    setLocalSpeedDirect(speed);
  }
  void GameImpl::setLocalSpeedDirect(int speed)
  {
    if (speed < 0)
    {
      // Reset the speed if it is negative
      for (int i = 0; i < BW::NUM_SPEEDS; ++i)
      {
        bwgame.setGameSpeedModifiers(i, BW::OriginalSpeedModifiers[i]);
        bwgame.setAltSpeedModifiers(i, BW::OriginalSpeedModifiers[i] * 3);
      }
    }
    else
    {
      // Set all speeds if it is positive
      for (int i = 0; i < BW::NUM_SPEEDS; ++i)
      {
        bwgame.setGameSpeedModifiers(i, speed);
        bwgame.setAltSpeedModifiers(i, speed * 3);
      }
    }
  }
  void GameImpl::setFrameSkip(int frameSkip)
  {
    setLastError(Errors::None);
    if ( !this->tournamentCheck(Tournament::SetFrameSkip, &frameSkip) )
      return;

    if ( frameSkip > 0 )
    {
      bwgame.setFrameSkip(frameSkip);
      return;
    }
    setLastError(Errors::Invalid_Parameter);
  }
  //------------------------------------------ ISSUE COMMAND -------------------------------------------------
  bool GameImpl::issueCommand(const Unitset& units, UnitCommand command)
  {
    std::vector< std::vector<Unit> > groupsOf12;
    std::vector<Unit> nextGroup;

    // Iterate the set of units
    for (Unit u : units)
    {
      // Skip on invalid units that can't issue commands
      if ( !u->exists() )
        continue;

      // If unit can't issue the command while grouped (e.g. if it is a building) then try to issue
      // it individually instead.
      if ( !u->canIssueCommandGrouped(command) )
      {
        u->issueCommand(command);
        continue;
      }

      // If the command optimizer has taken over the command, then don't add it to this group
      if ( static_cast<UnitImpl*>(u)->prepareIssueCommand(command) )
        continue;

      // Insert the unit into the next group
      nextGroup.push_back(u);

      // Create a new group of 12
      if ( nextGroup.size() >= 12 )
      {
        groupsOf12.push_back(nextGroup);
        nextGroup.clear();
      }
    }

    // Insert the last group into the groups of 12, if it is an incomplete group
    if ( !nextGroup.empty() )
      groupsOf12.push_back(nextGroup);

    // Return if no units to command
    if ( groupsOf12.empty() )
      return false;

    // Iterate our groups of 12
    for ( auto &v : groupsOf12 )
    {
      // Get the first unit available
      command.unit  = v.front();

      // Command optimization (no select) for unit unloading, but only if optimizer level >= 2
      // TODO this piece should be extracted to the CommandOptimizer
      if ( command.type != BWAPI::UnitCommandTypes::Unload || commandOptimizer.level < 2 )
      {
        // Select the unit group
        BW::Orders::Select sel(v);
        bwgame.QueueCommand(&sel, sel.size());
        apmCounter.addSelect();
      }

      // Execute the command
      BroodwarImpl.executeCommand( command );
    }
    return true;
  }
  //------------------------------------------ GET SELECTED UNITS --------------------------------------------
  const Unitset& GameImpl::getSelectedUnits() const
  {
    this->setLastError();
    if ( !this->isFlagEnabled(BWAPI::Flag::UserInput) )
    {
      this->setLastError(Errors::Access_Denied);
      return Unitset::none;
    }
    return selectedUnitSet;
  }
  //----------------------------------------------------- SELF -----------------------------------------------
  Player GameImpl::self() const
  {
    return this->BWAPIPlayer;
  }
  //----------------------------------------------------- ENEMY ----------------------------------------------
  Player GameImpl::enemy() const
  {
    return this->enemyPlayer;
  }
  //----------------------------------------------------- NEUTRAL --------------------------------------------
  Player GameImpl::neutral() const
  {
    return players[11];
  }
  //----------------------------------------------------- ALLIES ---------------------------------------------
  Playerset& GameImpl::allies()
  {
    return _allies;
  }
  //----------------------------------------------------- ENEMIES --------------------------------------------
  Playerset& GameImpl::enemies()
  {
    return _enemies;
  }
  //---------------------------------------------------- OBSERVERS -------------------------------------------
  Playerset& GameImpl::observers()
  {
    return _observers;
  }
  //--------------------------------------------------- LATENCY ----------------------------------------------
  int GameImpl::getLatencyFrames() const
  {
    return bwgame.getLatencyFrames();
  }
  int GameImpl::getLatencyTime() const
  {
    return getLatencyFrames() * bwgame.gameSpeedModifiers(bwgame.GameSpeed());
  }
  int GameImpl::getRemainingLatencyFrames() const
  {
    return getLatencyFrames() - (this->elapsedTime() - bwgame.lastTurnFrame());
  }
  int GameImpl::getRemainingLatencyTime() const
  {
    return getRemainingLatencyFrames() * bwgame.gameSpeedModifiers(bwgame.GameSpeed());
  }
  //--------------------------------------------------- VERSION ----------------------------------------------
  int GameImpl::getRevision() const
  {
    return SVN_REV;
  }
  int GameImpl::getClientVersion() const
  {
    return CLIENT_VERSION;
  }
  bool GameImpl::isDebug() const
  {
    return BUILD_DEBUG == 1;
  }
  //----------------------------------------------- LAT COMPENSATION -----------------------------------------
  bool GameImpl::isLatComEnabled() const
  {
    return data->hasLatCom;
  }
  void GameImpl::setLatCom(bool isEnabled)
  {
    if ( !this->tournamentCheck(Tournament::SetLatCom, &isEnabled) )
      return;
    data->hasLatCom = isEnabled;
  }
  //----------------------------------------------- GET INSTANCE ID ------------------------------------------
  int GameImpl::getInstanceNumber() const
  {
    return bwgame.getInstanceNumber();
  }
  //---------------------------------------------------- GET APM ---------------------------------------------
  int GameImpl::getAPM(bool includeSelects) const
  {
    return apmCounter.apm(includeSelects);
  }
  //---------------------------------------------------- SET MAP ---------------------------------------------
  bool GameImpl::setMap(const char *mapFileName)
  {
    if ( !mapFileName || !mapFileName[0] )
      return setLastError(Errors::Invalid_Parameter);

    if ( !std::ifstream(mapFileName).is_open() )
      return setLastError(Errors::File_Not_Found);

    if ( !this->tournamentCheck(Tournament::SetMap, (void*)mapFileName) )
      return setLastError(Errors::None);

    if ( !bwgame.setMapFileName(mapFileName) )
      return setLastError(Errors::Invalid_Parameter);

    return setLastError(Errors::None);
  }
  //------------------------------------------------- ELAPSED TIME -------------------------------------------
  int GameImpl::elapsedTime() const
  {
    return bwgame.elapsedTime();
  }
  //-------------------------------------- SET COMMAND OPTIMIZATION LEVEL ------------------------------------
  void GameImpl::setCommandOptimizationLevel(int level)
  {
    level = Util::clamp(level, 0, 4);
    if ( !this->tournamentCheck(Tournament::SetCommandOptimizationLevel, &level) )
      return;
    this->commandOptimizer.level = level;
  }
  //----------------------------------------------- COUNTDOWN TIMER ------------------------------------------
  int GameImpl::countdownTimer() const
  {
    return bwgame.countdownTimer();
  }
  //------------------------------------------------- GET REGION AT ------------------------------------------
  BWAPI::Region GameImpl::getRegionAt(int x, int y) const
  {
    this->setLastError();
    if ( !Position(x, y) )
    {
      this->setLastError(BWAPI::Errors::Invalid_Parameter);
      return nullptr;
    }
    BW::Region rgn = bwgame.getRegionAt(x,y);
    if ( !rgn )
    {
      this->setLastError(BWAPI::Errors::Invalid_Parameter);
      return nullptr;
    }
    return getRegion(rgn.getIndex());
  }
  int GameImpl::getLastEventTime() const
  {
    return this->lastEventTime;
  }
  bool GameImpl::setRevealAll(bool reveal)
  {
    if ( !isReplay() )
      return this->setLastError(Errors::Invalid_Parameter);
    bwgame.setReplayRevealAll(reveal);
    return this->setLastError();
  }
  //-------------------------------------------------- GET RANDOM SEED ---------------------------------------
  unsigned GameImpl::getRandomSeed() const
  {
    return bwgame.ReplayHead_gameSeed_randSeed();
  }
  void GameImpl::setCharacterName(const std::string& name)
  {
    if (isInGame()) return;
    bwgame.setCharacterName(name);
  }
  void GameImpl::setGameType(GameType gameType)
  {
    if (isInGame()) return;
    if (gameType == GameTypes::Melee) bwgame.setGameTypeMelee();
    else if (gameType == GameTypes::Use_Map_Settings) bwgame.setGameTypeUseMapSettings();
    else throw std::runtime_error("setGameType: the specified game type is currently not supported :(");
  }
  void GameImpl::setAIModule(AIModule* module)
  {
    this->specifiedModule = module;
  }
  void GameImpl::createSinglePlayerGame(std::function<void()> setupFunction)
  {
    bwgame.createSinglePlayerGame([&]() {
      bwgame.enableCheats();
      playerSet.clear();
      BWAPIPlayer = nullptr;
      for (int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
      {
        lastKnownRaceBeforeStart[i] = Race(bwgame.getPlayer(i).nRace());
        playerSet.insert(this->players[i]);
        if (i == bwgame.g_LocalHumanID()) BWAPIPlayer = this->players[i];
      }
      setupFunction();
      for (int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i) {
        lastKnownRaceBeforeStart[i] = Race(bwgame.getPlayer(i).pickedRace());
      }
      playerSet.clear();
      BWAPIPlayer = nullptr;
    });
  }
  void GameImpl::createMultiPlayerGame(std::function<void()> setupFunction)
  {
    bwgame.createMultiPlayerGame([&]() {
      playerSet.clear();
      BWAPIPlayer = nullptr;
      for (int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
      {
        lastKnownRaceBeforeStart[i] = Race(bwgame.getPlayer(i).nRace());
        this->players[i]->setID(i);
        playerSet.insert(this->players[i]);
        if (i == bwgame.g_LocalHumanID()) BWAPIPlayer = this->players[i];
      }
      setupFunction();
      for (int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
      {
        this->players[i]->setID(-1);
        lastKnownRaceBeforeStart[i] = Race(bwgame.getPlayer(i).pickedRace());
      }
      playerSet.clear();
      BWAPIPlayer = nullptr;
    });
  }
  void GameImpl::startGame()
  {
    bwgame.startGame();
  }
  void GameImpl::switchToPlayer(Player p)
  {
    if (!isInGame()) {
      bwgame.switchToSlot(((PlayerImpl*)p)->getIndex());
    }
  }
  int GameImpl::connectedPlayerCount()
  {
    return bwgame.connectedPlayerCount();
  }
  Unit GameImpl::createUnit(Player player, UnitType type, Position pos)
  {
    UnitImpl* u = getUnitFromBWUnit(bwgame.createUnit(((PlayerImpl*)player)->getIndex(), type.getID(), pos.x, pos.y));
    if (u) {
      u->updateInternalData();
    }
    return u;
  }
  void GameImpl::killUnit(Unit u)
  {
    bwgame.killUnit(((UnitImpl*)u)->bwunit);
  }
  void GameImpl::removeUnit(Unit u)
  {
    bwgame.removeUnit(((UnitImpl*)u)->bwunit);
  }

  namespace {

  template<bool default_little_endian = true, bool bounds_checking = true>
  struct data_reader {
    const uint8_t* ptr = nullptr;
    const uint8_t* begin = nullptr;
    const uint8_t* end = nullptr;
    data_reader() = default;
    data_reader(const uint8_t* ptr, const uint8_t* end) : ptr(ptr), begin(ptr), end(end) {}
    template<typename T, bool little_endian = default_little_endian>
    T get() {
      if (bounds_checking && left() < sizeof(T)) throw std::runtime_error("data_reader: attempt to read past end");
      union endian_t {uint32_t a; uint8_t b;};
      const bool native_little_endian = (endian_t{1}).b == 1;
      if (little_endian == native_little_endian) {
        T r;
        memcpy(&r, ptr, sizeof(T));
        ptr += sizeof(T);
        return r;
      } else {
        T r = 0;
        for (size_t i = 0; i < sizeof(T); ++i) {
          r |= (T)ptr[i] << ((little_endian ? i : sizeof(T) - 1 - i) * 8);
        }
        ptr += sizeof(T);
        return r;
      }
    }
    void skip(size_t n) {
      if (bounds_checking && left() < n) throw std::runtime_error("data_reader: attempt to seek past end");
      ptr += n;
    }
    void seek(size_t offset) {
      if (offset > size()) throw std::runtime_error("data_reader: attempt to seek past end");
      ptr = begin + offset;
    }
    size_t left() const {
      return end - ptr;
    }
    size_t size() const {
      return (size_t)(end - begin);
    }
    size_t tell() const {
      return (size_t)(ptr - begin);
    }
    std::string get_string() {
      size_t n = get<uint32_t>();
      std::string r;
      r.reserve(n);
      for (;n; --n) r += get<char>();
      return r;
    }
  };

  using data_reader_le = data_reader<true>;
  using data_reader_be = data_reader<false>;

  template<bool little_endian, typename T>
  static inline void set_value_at(uint8_t* ptr, T value) {
    static_assert(std::is_integral<T>::value, "can only write integers");
    union endian_t {uint32_t a; uint8_t b;};
    const bool native_little_endian = (endian_t{1}).b == 1;
    if (little_endian == native_little_endian) {
        memcpy(ptr, &value, sizeof(T));
    } else {
      for (size_t i = 0; i < sizeof(T); ++i) {
        ptr[i] = value >> ((little_endian ? i : sizeof(T) - 1 - i) * 8);
      }
    }
  }

  template<bool default_little_endian = true>
  struct dynamic_writer {
    std::vector<uint8_t> vec;
    size_t pos = 0;
    dynamic_writer() = default;
    dynamic_writer(size_t initial_size) : vec(initial_size) {}
    template<typename T, bool little_endian = default_little_endian>
    void put(T v) {
      static_assert(std::is_integral<T>::value, "don't know how to write this type");
      size_t n = pos;
      skip(sizeof(T));
      set_value_at<little_endian>(data() + n, v);
    }
    void skip(size_t n) {
      pos += n;
      if (pos >= vec.size()) {
        if (vec.size() < 2048) vec.resize(std::max(pos, vec.size() + vec.size()));
        else vec.resize(std::max(pos, std::max(vec.size() + vec.size() / 2, (size_t)32)));
      }
    }
    void put_bytes(const uint8_t* src, size_t n) {
      skip(n);
      memcpy(data() + pos - n, src, n);
    }
    size_t size() const {
      return pos;
    }
    const uint8_t* data() const {
      return vec.data();
    }
    uint8_t* data() {
      return vec.data();
    }
    void put_string(const std::string& str) {
      if ((size_t)(uint32_t)str.size() != str.size()) throw std::runtime_error("put_string: string too big");
      put<uint32_t>((uint32_t)str.size());
      const char* c = str.data();
      const char* e = c + str.size();
      while (c != e) {
        put(*c);
        ++c;
      }
    }
  };

  }

  void GameImpl::onCustomAction(int player, const char* data, size_t size) {
    data_reader_le r((const uint8_t*)data, (const uint8_t*)data + size);

    int id = r.get<uint8_t>();

    if (id == 1) {
      std::string id = r.get_string();
      saveSnapshotAction(std::move(id));
    } else if (id == 2) {
      std::string id = r.get_string();
      loadSnapshotAction(std::move(id));
    } else if (id == 3) {
      std::string id = r.get_string();
      deleteSnapshotAction(std::move(id));
    }
  }

  void GameImpl::saveSnapshot(std::string id)
  {
    dynamic_writer<> w;
    w.put<uint8_t>(1);
    w.put_string(id);
    bwgame.sendCustomAction(w.data(), w.size());
  }

  void GameImpl::loadSnapshot(const std::string &id)
  {
    dynamic_writer<> w;
    w.put<uint8_t>(2);
    w.put_string(id);
    bwgame.sendCustomAction(w.data(), w.size());
  }

  void GameImpl::deleteSnapshot(const std::string &id)
  {
    dynamic_writer<> w;
    w.put<uint8_t>(3);
    w.put_string(id);
    bwgame.sendCustomAction(w.data(), w.size());
  }

  void GameImpl::saveSnapshotAction(std::string id)
  {
    Snapshot s;
    s.bwsnapshot = bwgame.saveSnapshot();
    s.players.resize(BW::PLAYER_COUNT);
    for (size_t i = 0; i != BW::PLAYER_COUNT; ++i) {
      auto& src = *players[i];
      auto& dst = s.players[i];
      dst.data = src.data;
      dst._repairedMinerals = src._repairedMinerals;
      dst._repairedGas = src._repairedGas;
      dst._refundedMinerals = src._refundedMinerals;
      dst._refundedGas = src._refundedGas;
      dst.wasSeenByBWAPIPlayer = src.wasSeenByBWAPIPlayer;
    }
    s.frameCount = frameCount;
    s.flags = flags;
    s.commandOptimizerLevel = commandOptimizer.level;
    s.cheatFlags = cheatFlags;
    snapshots[std::move(id)] = std::move(s);
  }

  void GameImpl::loadSnapshotAction(const std::string &id)
  {
    auto i = snapshots.find(id);
    if (i == snapshots.end()) throw std::runtime_error("no such snapshot");
    auto& s = i->second;
    bwgame.loadSnapshot(s.bwsnapshot);

    for (size_t i = 0; i != BW::PLAYER_COUNT; ++i) {
      auto& dst = *players[i];
      auto& src = s.players[i];
      dst.data = src.data;
      dst._repairedMinerals = src._repairedMinerals;
      dst._repairedGas = src._repairedGas;
      dst._refundedMinerals = src._refundedMinerals;
      dst._refundedGas = src._refundedGas;
      dst.wasSeenByBWAPIPlayer = src.wasSeenByBWAPIPlayer;

      dst.units.clear();
    }

    this->aliveUnits.clear();
    this->dyingUnits.clear();
    this->discoverUnits.clear();
    this->accessibleUnits.clear();
    this->evadeUnits.clear();
    this->selectedUnitSet.clear();
    this->startLocations.clear();
    this->playerSet.clear();
    this->minerals.clear();
    this->geysers.clear();
    this->neutralUnits.clear();
    this->bullets.clear();
    this->pylons.clear();
    this->staticMinerals.clear();
    this->staticGeysers.clear();
    this->staticNeutralUnits.clear();
    this->_allies.clear();
    this->_enemies.clear();
    this->_observers.clear();

    this->savedUnitSelection.fill(nullptr);
    this->wantSelectionUpdate = false;

    frameCount = s.frameCount;
    flags = s.flags;

    for(unsigned int j = 0; j < this->commandBuffer.size(); ++j)
      for (unsigned int i = 0; i < this->commandBuffer[j].size(); ++i)
        delete this->commandBuffer[j][i];
    this->commandBuffer.clear();
    this->commandBuffer.reserve(16);

    commandOptimizer.init();
    commandOptimizer.level = s.commandOptimizerLevel;

    for ( Unitset::iterator d = this->deadUnits.begin(); d != this->deadUnits.end(); ++d )
      delete static_cast<UnitImpl*>(*d);
    this->deadUnits.clear();

    for (UnitImpl* u : unitArray)
    {
      if (!u) continue;
      u->clear();
      u->userSelected = false;
      u->isAlive = false;
      u->wasAlive = false;
      u->wasCompleted = false;
      u->wasAccessible = false;
      u->wasVisible = false;
      u->nukeDetected = false;
      u->lastType = UnitTypes::Unknown;
      u->lastPlayer = nullptr;

      u->setID(-1);
    }

    this->bulletNextId = 0;
    this->cheatFlags = s.cheatFlags;

    updateUnits();
    updateUnits();
    events.clear();
  }

  void GameImpl::deleteSnapshotAction(const std::string &id)
  {
    auto i = snapshots.find(id);
    if (i != snapshots.end()) snapshots.erase(i);
  }

  std::vector<std::string> GameImpl::listSnapshots()
  {
    std::vector<std::string> r;
    for (auto& v : snapshots) r.push_back(v.first);
    return r;
  }

  void GameImpl::setRandomSeed(uint32_t value)
  {
    bwgame.setRandomSeed(value);
  }

  void GameImpl::disableTriggers()
  {
    bwgame.disableTriggers();
  }

  BWAPI::Position GameImpl::getScreenSize() const
  {
    return {bwgame.screenWidth(), bwgame.screenHeight()};
  }
  std::tuple<int, int, uint32_t*> GameImpl::drawGameScreen(int x, int y, int width, int height)
  {
    return bwgame.drawGameScreen(x, y, width, height);
  }

}

