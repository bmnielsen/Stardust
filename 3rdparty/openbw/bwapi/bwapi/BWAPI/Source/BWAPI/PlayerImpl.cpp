#include "PlayerImpl.h"
#include "GameImpl.h"
#include "UnitImpl.h"

#include <string>
#include <Util/Convenience.h>

#include <BW/BWData.h>

#include <BWAPI/PlayerType.h>

#include "../../../Debug.h"

namespace BWAPI
{
  //--------------------------------------------- CONSTRUCTOR ------------------------------------------------
  PlayerImpl::PlayerImpl(u8 index, BW::Player bwplayer)
    : bwplayer(std::move(bwplayer)), index(index)
  {
    resetResources();
    self->color = index < 12 ? bwplayer.playerColorIndex() : 0;
  }
  //--------------------------------------------- SET ID -----------------------------------------------------
  void PlayerImpl::setID(int newID)
  {
    id = newID;
  }
  //--------------------------------------------- GET INDEX --------------------------------------------------
  int PlayerImpl::getIndex() const
  {
    return index;
  }
  //--------------------------------------------- GET NAME ---------------------------------------------------
  std::string PlayerImpl::getName() const
  {
    if ( index == 11 )
      return std::string("Neutral");
    return std::string(bwplayer.szName());
  }
  //--------------------------------------------- GET RACE ---------------------------------------------------
  BWAPI::Race PlayerImpl::getRace() const
  {
    BroodwarImpl.setLastError();
    if ( this->index < BW::PLAYABLE_PLAYER_COUNT )
    {
      Race rlast = BroodwarImpl.lastKnownRaceBeforeStart[this->index];
      if (  rlast != Races::Zerg          &&
            rlast != Races::Terran        &&
            rlast != Races::Protoss       &&
            !this->wasSeenByBWAPIPlayer   && 
            !BroodwarImpl.isFlagEnabled(Flag::CompleteMapInformation) )
      {
        BroodwarImpl.setLastError(Errors::Access_Denied);
        return Races::Unknown;
      }
    }
    return BWAPI::Race( bwplayer.nRace() );
  }
  //--------------------------------------------- GET TYPE ---------------------------------------------------
  BWAPI::PlayerType PlayerImpl::getType() const
  {
    return BWAPI::PlayerType((int)(bwplayer.nType()));
  }
  //--------------------------------------------- GET FORCE --------------------------------------------------
  Force PlayerImpl::getForce() const
  {
    return force;
  }
  //--------------------------------------------- IS ALLIES WITH ---------------------------------------------
  bool PlayerImpl::isAlly(const Player player) const
  {
    if ( !player || this->isNeutral() || player->isNeutral() || this->isObserver() || player->isObserver() )
      return false;
    return bwplayer.playerAlliances(static_cast<PlayerImpl*>(player)->getIndex()) != 0;
  }
  //--------------------------------------------- IS ALLIES WITH ---------------------------------------------
  bool PlayerImpl::isEnemy(const Player player) const
  {
    if ( !player || this->isNeutral() || player->isNeutral() || this->isObserver() || player->isObserver() )
      return false;
    return bwplayer.playerAlliances(static_cast<PlayerImpl*>(player)->getIndex()) == 0;
  }
  //--------------------------------------------- IS NEUTRAL -------------------------------------------------
  bool PlayerImpl::isNeutral() const
  {
    return index == 11;
  }
  //--------------------------------------------- GET START POSITION -----------------------------------------
  TilePosition PlayerImpl::getStartLocation() const
  {
    // Clear last error
    BroodwarImpl.setLastError();

    // Return None if there is no start location
    if (index >= BW::PLAYABLE_PLAYER_COUNT || bwplayer.startPosition() == BW::Positions::Origin)
      return TilePositions::None;

    // Return unknown and set Access_Denied if the start location
    // should not be made available.
    if ( !BroodwarImpl.isReplay() &&
       BroodwarImpl.self()->isEnemy(const_cast<PlayerImpl*>(this)) &&
       !BroodwarImpl.isFlagEnabled(Flag::CompleteMapInformation) )
    {
      BroodwarImpl.setLastError(Errors::Access_Denied);
      return TilePositions::Unknown;
    }
    // return the start location as a tile position
    return TilePosition(bwplayer.startPosition() - BW::Position((TILE_SIZE * 4) / 2, (TILE_SIZE * 3) / 2));
  }
  //--------------------------------------------- IS VICTORIOUS ----------------------------------------------
  bool PlayerImpl::isVictorious() const
  {
    if ( index >= 8 ) 
      return false;
    return bwplayer.PlayerVictory() == 3;
  }
  //--------------------------------------------- IS DEFEATED ------------------------------------------------
  bool PlayerImpl::isDefeated() const
  {
    if ( index >= 8 ) 
      return false;
    return bwplayer.PlayerVictory() == 1 ||
           bwplayer.PlayerVictory() == 2 ||
           bwplayer.PlayerVictory() == 4 ||
           bwplayer.PlayerVictory() == 6;
  }
  //--------------------------------------------- UPDATE -----------------------------------------------------
  void PlayerImpl::updateData()
  { 
    self->color = index < BW::PLAYER_COUNT ? bwplayer.playerColorIndex() : 0;
  
    // Get upgrades, tech, resources
    if ( this->isNeutral() || 
      index >= BW::PLAYER_COUNT ||
         (!BroodwarImpl.isReplay() && 
          BroodwarImpl.self()->isEnemy(this) && 
          !BroodwarImpl.isFlagEnabled(Flag::CompleteMapInformation)) )
    {
      self->minerals           = 0;
      self->gas                = 0;
      self->gatheredMinerals   = 0;
      self->gatheredGas        = 0;
      self->repairedMinerals   = 0;
      self->repairedGas        = 0;
      self->refundedMinerals   = 0;
      self->refundedGas        = 0;

      // Reset values
      MemZero(self->upgradeLevel);
      MemZero(self->hasResearched);
      MemZero(self->isUpgrading);
      MemZero(self->isResearching);
    
      MemZero(self->maxUpgradeLevel);
      MemZero(self->isResearchAvailable);
      MemZero(self->isUnitAvailable);

      if (!this->isNeutral() && index < BW::PLAYER_COUNT)
      {
        // set upgrade level for visible enemy units
        for(int i = 0; i < BW::UPGRADE_TYPE_COUNT; ++i)
        {
          for(UnitType t : UpgradeType(i).whatUses())
          {
            if ( self->completedUnitCount[t] > 0 )
              self->upgradeLevel[i] = bwplayer.currentUpgradeLevel(i);
          }
        }
      }
    }
    else
    {
      this->wasSeenByBWAPIPlayer = true;

      // set resources
      self->minerals           = bwplayer.minerals();
      self->gas                = bwplayer.gas();
      self->gatheredMinerals   = bwplayer.cumulativeMinerals();
      self->gatheredGas        = bwplayer.cumulativeGas();
      self->repairedMinerals   = this->_repairedMinerals;
      self->repairedGas        = this->_repairedGas;
      self->refundedMinerals   = this->_refundedMinerals;
      self->refundedGas        = this->_refundedGas;

      // set upgrade level
      for(int i = 0; i < BW::UPGRADE_TYPE_COUNT; ++i)
      {
        self->upgradeLevel[i]     = bwplayer.currentUpgradeLevel(i);
        self->maxUpgradeLevel[i]  = bwplayer.maxUpgradeLevel(i);
      }

      // set abilities researched
      for(int i = 0; i < BW::TECH_TYPE_COUNT; ++i)
      {
        self->hasResearched[i]        = TechType(i).whatResearches() == UnitTypes::None ? true : bwplayer.techResearched(i);
        self->isResearchAvailable[i]  = bwplayer.techAvailable(i);
      }

      // set upgrades in progress
      for (int i = 0; i < BW::UPGRADE_TYPE_COUNT; ++i)
        self->isUpgrading[i]   = bwplayer.upgradeInProgress(i);
      
      // set research in progress
      for (int i = 0; i < BW::TECH_TYPE_COUNT; ++i)
        self->isResearching[i] = bwplayer.techResearchInProgress(i);

      for (int i = 0; i < BW::UNIT_TYPE_COUNT; ++i)
        self->isUnitAvailable[i] = bwplayer.unitAvailability(i);

      self->hasResearched[TechTypes::Enum::Nuclear_Strike] = self->isUnitAvailable[UnitTypes::Enum::Terran_Nuclear_Missile];
    }

    // Get Scores, supply
    if ( (!BroodwarImpl.isReplay() && 
          BroodwarImpl.self()->isEnemy(this) && 
          !BroodwarImpl.isFlagEnabled(Flag::CompleteMapInformation)) ||
          index >= BW::PLAYER_COUNT)
    {
      MemZero(self->supplyTotal);
      MemZero(self->supplyUsed);
      MemZero(self->deadUnitCount);
      MemZero(self->killedUnitCount);

      self->totalUnitScore      = 0;
      self->totalKillScore      = 0;
      self->totalBuildingScore  = 0;
      self->totalRazingScore    = 0;
      self->customScore         = 0;
    }
    else
    {
      // set supply
      for (u8 i = 0; i < BW::RACE_COUNT; ++i)
      {
        self->supplyTotal[i]  = bwplayer.suppliesAvailable(i);
        if (self->supplyTotal[i] > bwplayer.suppliesMax(i))
          self->supplyTotal[i]  = bwplayer.suppliesMax(i);
        self->supplyUsed[i]   = bwplayer.suppliesUsed(i);
      }
      // set total unit counts
      for (int i = 0; i < BW::UNIT_TYPE_COUNT; ++i)
      {
        self->deadUnitCount[i]   = bwplayer.unitCountsDead(i);
        self->killedUnitCount[i] = bwplayer.unitCountsKilled(i);
      }
      // set macro dead unit counts
      self->deadUnitCount[UnitTypes::AllUnits]    = bwplayer.allUnitsLost() + bwplayer.allBuildingsLost();
      self->deadUnitCount[UnitTypes::Men]         = bwplayer.allUnitsLost();
      self->deadUnitCount[UnitTypes::Buildings]   = bwplayer.allBuildingsLost();
      self->deadUnitCount[UnitTypes::Factories]   = bwplayer.allFactoriesLost();

      // set macro kill unit counts
      self->killedUnitCount[UnitTypes::AllUnits]  = bwplayer.allUnitsKilled() + bwplayer.allBuildingsRazed();
      self->killedUnitCount[UnitTypes::Men]       = bwplayer.allUnitsKilled();
      self->killedUnitCount[UnitTypes::Buildings] = bwplayer.allBuildingsRazed();
      self->killedUnitCount[UnitTypes::Factories] = bwplayer.allFactoriesRazed();
      
      // set score counts
      self->totalUnitScore      = bwplayer.allUnitScore();
      self->totalKillScore      = bwplayer.allKillScore();
      self->totalBuildingScore  = bwplayer.allBuildingScore();
      self->totalRazingScore    = bwplayer.allRazingScore();
      self->customScore         = bwplayer.customScore();
    }

    if (index < BW::PLAYER_COUNT && (bwplayer.nType() == PlayerTypes::PlayerLeft ||
        bwplayer.nType() == PlayerTypes::ComputerLeft ||
       (bwplayer.nType() == PlayerTypes::Neutral && !isNeutral())))
    {
      self->leftGame = true;
    }
  }
  //----------------------------------------------------------------------------------------------------------
  void PlayerImpl::onGameEnd()
  {
    this->units.clear();
    this->clientInfo.clear();
    this->interfaceEvents.clear();

    self->leftGame = false;
    this->wasSeenByBWAPIPlayer = false;
  }
  void PlayerImpl::setParticipating(bool isParticipating)
  {
    self->isParticipating = isParticipating;
  }
  void PlayerImpl::resetResources()
  {
    _repairedMinerals = 0;
    _repairedGas      = 0;
    _refundedMinerals = 0;
    _refundedGas      = 0;
  }
  void PlayerImpl::setRace(Race race)
  {
    if (BroodwarImpl.isInGame()) return;
    bwplayer.setRace((int)race);
  }
  void PlayerImpl::closeSlot()
  {
    if (BroodwarImpl.isInGame()) return;
    bwplayer.closeSlot();
  }
  void PlayerImpl::openSlot()
  {
    if (BroodwarImpl.isInGame()) return;
    bwplayer.openSlot();
  }
  void PlayerImpl::setUpgradeLevel(UpgradeType upgrade, int level)
  {
    bwplayer.setUpgradeLevel((int)upgrade, level);
  }

  void PlayerImpl::setResearched(TechType tech, bool researched)
  {
    bwplayer.setResearched((int)tech, researched);
  }

  void PlayerImpl::setMinerals(int value)
  {
    bwplayer.setMinerals(value);
  }

  void PlayerImpl::setGas(int value)
  {
    bwplayer.setGas(value);
  }
}
