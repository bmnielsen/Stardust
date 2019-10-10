#include "TestMap1.h"
#include "BWAssert.h"
using namespace BWAPI;
int assert_success_count;
int assert_fail_count;

#define findAndRemove(S,V)\
{\
  bool found = false;\
  for(auto i = S.begin(); i != S.end(); ++i)\
  {\
    if (*i == V)\
    {\
      found = true;\
      S.erase(i);\
      break;\
    }\
  }\
  if (!found)\
    BWAssert(false);\
}
void findAndRemoveType(Unitset &s, UnitType t)
{
  auto it = std::find_if(s.begin(), s.end(), BWAPI::Filter::GetType == t);
  if (it != s.end())
  {
    s.erase(it);
  }
  else
  {
    BWAssert(false);
  }
}

void TestMap1::onStart()
{
  assert_success_count=0;
  assert_fail_count=0;
  BWAssert(Broodwar->mapFileName()=="testmap1.scm");
  BWAssert(Broodwar->mapWidth()==128);
  BWAssert(Broodwar->mapHeight()==128);
  for(int x=0;x<128;++x)
  {
    BWAssert(Broodwar->isBuildable(x,0)==true);
  }
  for(int x=0;x<512;++x)
  {
    BWAssert(Broodwar->isWalkable(x,0)==true);
  }
  BWAssert(Broodwar->isFlagEnabled(Flag::CompleteMapInformation)==false);
  BWAssert(Broodwar->isFlagEnabled(Flag::UserInput)==false);
  BWAssert(Broodwar->getScreenPosition()==Positions::Unknown);
  BWAssert(Broodwar->getMousePosition()==Positions::Unknown);
  BWAssert(Broodwar->isInGame()==true);
  BWAssert(Broodwar->isMultiplayer()==false);
  BWAssert(Broodwar->isPaused()==false);    // Why would this fail?
  BWAssert(Broodwar->isReplay()==false);
  BWAssert(Broodwar->getFrameCount()==0);
  BWAssert(Broodwar->self()!=NULL);
  BWAssert(Broodwar->enemy()!=NULL);
  BWAssert(Broodwar->self()!=Broodwar->enemy());

  BWAssert(Broodwar->getPlayers().size()==3);
  Playerset players = Broodwar->getPlayers();
  findAndRemove(players,Broodwar->self());
  findAndRemove(players,Broodwar->enemy());
  BWAssert(players.size()==1);
  Player neutral = *players.begin();
  BWAssert(neutral->isNeutral()==true);

  BWAssert(Broodwar->self()->getUnits().size()==24);
  Unitset units = Broodwar->self()->getUnits();
  for (Unit unit : units)
  {
    BWAssert(unit->exists()==true);
    BWAssert(unit->isAccelerating()==false);
    BWAssert(unit->isAttacking()==false);
    BWAssert(unit->isBeingConstructed()==false);
    BWAssert(unit->isBeingGathered()==false);
    BWAssert(unit->isBeingHealed()==false);
    BWAssert(unit->isBlind()==false);
    BWAssert(unit->isBraking()==false);
    BWAssert(unit->isBurrowed()==false);
    BWAssert(unit->isCarryingGas()==false);
    BWAssert(unit->isCarryingMinerals()==false);
    BWAssert(unit->isCloaked()==false);
    BWAssert(unit->isCompleted()==true);
    BWAssert(unit->isConstructing()==false);
    BWAssert(unit->isDefenseMatrixed()==false);
    BWAssert(unit->isEnsnared()==false);
    BWAssert(unit->isFollowing()==false);
    BWAssert(unit->isGatheringGas()==false);
    BWAssert(unit->isGatheringMinerals()==false);
    BWAssert(unit->isHallucination()==false);
    BWAssert(unit->isIdle()==true);
    BWAssert(unit->isIrradiated()==false);
    BWAssert(unit->isLifted()==false);
    BWAssert(unit->isLoaded()==false);
    BWAssert(unit->isLockedDown()==false);
    BWAssert(unit->isMaelstrommed()==false);
    BWAssert(unit->isMorphing()==false);
    // It is normal for some units to initially have a move order.
    //BWAssert(unit->isMoving()==false);
    BWAssert(unit->isParasited()==false);
    BWAssert(unit->isPatrolling()==false);
    BWAssert(unit->isPlagued()==false);
    BWAssert(unit->isRepairing()==false);
    BWAssert(unit->isResearching()==false);
    BWAssert(unit->isSelected()==false);
    BWAssert(unit->isSieged()==false);
    BWAssert(unit->isStartingAttack()==false);
    BWAssert(unit->isStasised()==false);
    BWAssert(unit->isStimmed()==false);
    BWAssert(unit->isTraining()==false);
    BWAssert(unit->isUnderStorm()==false);
    BWAssert(unit->isPowered()==true);
    BWAssert(unit->isUpgrading()==false);
    BWAssert(unit->isVisible()==true);
  }
  findAndRemoveType(units,UnitTypes::Terran_Command_Center);
  findAndRemoveType(units,UnitTypes::Terran_Comsat_Station);
  findAndRemoveType(units,UnitTypes::Terran_Command_Center);
  findAndRemoveType(units,UnitTypes::Terran_Nuclear_Silo);
  findAndRemoveType(units,UnitTypes::Terran_Supply_Depot);
  findAndRemoveType(units,UnitTypes::Terran_Supply_Depot);
  findAndRemoveType(units,UnitTypes::Terran_Supply_Depot);
  findAndRemoveType(units,UnitTypes::Terran_Supply_Depot);
  findAndRemoveType(units,UnitTypes::Terran_Supply_Depot);
  findAndRemoveType(units,UnitTypes::Terran_Refinery);
  findAndRemoveType(units,UnitTypes::Terran_Barracks);
  findAndRemoveType(units,UnitTypes::Terran_Academy);
  findAndRemoveType(units,UnitTypes::Terran_Factory);
  findAndRemoveType(units,UnitTypes::Terran_Machine_Shop);
  findAndRemoveType(units,UnitTypes::Terran_Starport);
  findAndRemoveType(units,UnitTypes::Terran_Control_Tower);
  findAndRemoveType(units,UnitTypes::Terran_Science_Facility);
  findAndRemoveType(units,UnitTypes::Terran_Physics_Lab);
  findAndRemoveType(units,UnitTypes::Terran_Science_Facility);
  findAndRemoveType(units,UnitTypes::Terran_Covert_Ops);
  findAndRemoveType(units,UnitTypes::Terran_Engineering_Bay);
  findAndRemoveType(units,UnitTypes::Terran_Armory);
  findAndRemoveType(units,UnitTypes::Terran_Missile_Turret);
  findAndRemoveType(units,UnitTypes::Terran_Bunker);
  BWAssert(units.size()==0);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Command_Center)==2);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Comsat_Station)==1);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Nuclear_Silo)==1);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Supply_Depot)==5);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Refinery)==1);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Barracks)==1);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Academy)==1);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Factory)==1);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Machine_Shop)==1);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Starport)==1);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Control_Tower)==1);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Science_Facility)==2);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Physics_Lab)==1);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Covert_Ops)==1);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Engineering_Bay)==1);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Armory)==1);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Missile_Turret)==1);
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Bunker)==1);
  for (UnitType t : UnitTypes::allUnitTypes())
  {
    if (t.isBuilding() && t.getRace() == Races::Terran && !t.isSpecialBuilding() )
    {
      BWAssertF(Broodwar->self()->completedUnitCount(t)>0, { log("  No %s owned.", t.c_str()); });
    }
    else
    {
      BWAssertF(Broodwar->self()->completedUnitCount(t)==0, { log("  Extra %s owned.", t.c_str()); });
    }
    BWAssert(Broodwar->self()->incompleteUnitCount(t)==0);
    BWAssert(Broodwar->self()->deadUnitCount(t)==0);
    BWAssert(Broodwar->self()->killedUnitCount(t)==0);

    BWAssert(Broodwar->enemy()->completedUnitCount(t)==0);
    BWAssert(Broodwar->enemy()->incompleteUnitCount(t)==0);
    BWAssert(Broodwar->enemy()->deadUnitCount(t)==0);
    BWAssert(Broodwar->enemy()->killedUnitCount(t)==0);

    BWAssert(neutral->completedUnitCount(t)==0);
    BWAssert(neutral->incompleteUnitCount(t)==0);
    BWAssert(neutral->deadUnitCount(t)==0);
    BWAssert(neutral->killedUnitCount(t)==0);
  }
  for (TechType t : TechTypes::allTechTypes())
  {
    BWAssert(Broodwar->self()->isResearching(t)==false);
    BWAssert(Broodwar->enemy()->isResearching(t)==false);
    BWAssert(neutral->isResearching(t)==false);
    BWAssert(Broodwar->enemy()->hasResearched(t)==false);
    BWAssert(neutral->hasResearched(t)==false);
    if (t.whatResearches()==UnitTypes::None && t != TechTypes::None )
    {
      BWAssertErr(Broodwar->self()->hasResearched(t)==true, t.c_str());
    }
    else
    {
      BWAssertErr(Broodwar->self()->hasResearched(t)==false, t.c_str());
    }
  }
  for (UpgradeType u : UpgradeTypes::allUpgradeTypes())
  {
    BWAssert(Broodwar->self()->isUpgrading(u)==false);
    BWAssert(Broodwar->self()->getUpgradeLevel(u)==0);
    BWAssert(Broodwar->enemy()->isUpgrading(u)==false);
    BWAssert(Broodwar->enemy()->getUpgradeLevel(u)==0);
    BWAssert(neutral->isUpgrading(u)==false);
    BWAssert(neutral->getUpgradeLevel(u)==0);
  }
  BWAssert(Broodwar->self()->getRace()==Races::Terran);
  BWAssert(Broodwar->self()->getType()==PlayerTypes::Player);
  BWAssert(Broodwar->self()->minerals()==0);
  BWAssert(Broodwar->self()->gas()==0);
  BWAssert(Broodwar->self()->gatheredMinerals()==0);
  BWAssert(Broodwar->self()->gatheredGas()==0);
  BWAssert(Broodwar->self()->supplyTotal()==120);
  BWAssert(Broodwar->self()->supplyUsed()==0);

  BWAssert(Broodwar->enemy()->getRace()==Races::Protoss);
  BWAssert(Broodwar->enemy()->getType()==PlayerTypes::Computer);
  BWAssert(Broodwar->enemy()->minerals()==0);
  BWAssert(Broodwar->enemy()->gas()==0);
  BWAssert(Broodwar->enemy()->gatheredMinerals()==0);
  BWAssert(Broodwar->enemy()->gatheredGas()==0);
  BWAssert(Broodwar->enemy()->supplyTotal()==0);
  BWAssert(Broodwar->enemy()->supplyUsed()==0);

  BWAssert(Broodwar->enemy()->getUnits().size()==0);

  BWAssert(Broodwar->self()->isAlly(Broodwar->enemy())==false);
  BWAssert(Broodwar->self()->isAlly(neutral)==false);
  BWAssert(Broodwar->enemy()->isAlly(Broodwar->self())==false);
  BWAssert(Broodwar->enemy()->isAlly(neutral)==false);
  BWAssert(neutral->isAlly(Broodwar->self())==false);
  BWAssert(neutral->isAlly(Broodwar->enemy())==false);

  BWAssert(Broodwar->self()->isEnemy(Broodwar->enemy())==true);
  BWAssert(Broodwar->self()->isEnemy(neutral)==false);
  BWAssert(Broodwar->enemy()->isEnemy(Broodwar->self())==true);
  BWAssert(Broodwar->enemy()->isEnemy(neutral)==false);
  BWAssert(neutral->isEnemy(Broodwar->self())==false);
  BWAssert(neutral->isEnemy(Broodwar->enemy())==false);

  BWAssert(Broodwar->self()->isVictorious()==false);
  BWAssert(Broodwar->enemy()->isVictorious()==false);
  BWAssert(neutral->isVictorious()==false);

  BWAssert(Broodwar->self()->isDefeated()==false);
  BWAssert(Broodwar->enemy()->isDefeated()==false);
  BWAssert(neutral->isDefeated()==false);

  BWAssert(Broodwar->self()->leftGame()==false);
  BWAssert(Broodwar->enemy()->leftGame()==false);
  BWAssert(neutral->leftGame()==false);

  Broodwar->sendText("show me the money");
  Broodwar->sendText("operation cwal");
  Broodwar->setLocalSpeed(0);
  Broodwar->setFrameSkip(512);
}
void TestMap1::onEnd(bool isWinner)
{
}
void TestMap1::onFrame()
{
  Broodwar->drawTextScreen(0,0,"Assert success count: %d",assert_success_count);
  Broodwar->drawTextScreen(0,20,"Assert failed count: %d",assert_fail_count);
  int frame=Broodwar->getFrameCount();
  if (frame==100)
  {
    BWAssert(Broodwar->isInGame()==true);
    BWAssert(Broodwar->isMultiplayer()==false);
    BWAssert(Broodwar->isPaused()==false);
    BWAssert(Broodwar->isReplay()==false);
    BWAssert(Broodwar->self()->minerals()==10050);
    BWAssert(Broodwar->self()->gas()==10000);
    BWAssert(Broodwar->self()->gatheredMinerals()==50);
    BWAssert(Broodwar->self()->gatheredGas()==0);
    for (Unit u : Broodwar->self()->getUnits())
    {
      if (u->getType().isFlyingBuilding())
      {
        u->lift();
      }
    }
  }
  else if (frame==200)
  {
    for (Unit u : Broodwar->self()->getUnits())
    {
      if (u->getType()==UnitTypes::Terran_Command_Center ||
          u->getType()==UnitTypes::Terran_Barracks ||
          u->getType()==UnitTypes::Terran_Engineering_Bay ||
          u->getType()==UnitTypes::Terran_Factory ||
          u->getType()==UnitTypes::Terran_Starport ||
          u->getType()==UnitTypes::Terran_Science_Facility)
      {
        BWAssert(u->isLifted()==true);
        u->land(TilePosition(u->getTilePosition().x,u->getTilePosition().y+2));
      }
      else
      {
        BWAssert(u->isLifted()==false);
      }
    }
  }
  else if (frame==300)
  {
    for (Unit u : Broodwar->self()->getUnits())
    {
      BWAssert(u->isLifted()==false);
      BWAssert(u->isTraining()==false);
      if (u->getType()==UnitTypes::Terran_Command_Center)
        u->train(UnitTypes::Terran_SCV);
      if (u->getType()==UnitTypes::Terran_Barracks)
        u->train(UnitTypes::Terran_Marine);
      if (u->getType()==UnitTypes::Terran_Factory)
        u->train(UnitTypes::Terran_Vulture);
      if (u->getType()==UnitTypes::Terran_Starport)
        u->train(UnitTypes::Terran_Wraith);
    }
  }
  else if (frame==320)
  {
    for (Unit u : Broodwar->self()->getUnits())
    {
      BWAssert(u->isLifted()==false);
      if (u->getType()==UnitTypes::Terran_Command_Center ||
          u->getType()==UnitTypes::Terran_Barracks ||
          u->getType()==UnitTypes::Terran_Factory ||
          u->getType()==UnitTypes::Terran_Starport)
      {
        BWAssert(u->isTraining()==true);
        BWAssert(u->isIdle()==false);
        BWAssert(u->getTrainingQueue().size()==1);
      }
      else
      {
        BWAssert(u->isTraining()==false);
        BWAssert(u->isIdle()==true);
        BWAssert(u->getTrainingQueue().size()==0);
      }
    }
  }
  else if (frame==400)
  {
    for (Unit u : Broodwar->self()->getUnits())
    {
      BWAssert(u->isTraining()==false);
      BWAssert(u->isIdle()==true);
      BWAssert(u->getTrainingQueue().size()==0);
    }
    BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_SCV)==2);
    BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Marine)==1);
    BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Vulture)==1);
    BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Wraith)==1);
    BWAssert(Broodwar->self()->supplyUsed()==14);
    BWAssert(Broodwar->self()->minerals()==9675);
    BWAssert(Broodwar->self()->gas()==9900);
  }
  else if (frame==1000)
  {
    Broodwar->setLocalSpeed(-1);
  }
}
void TestMap1::onSendText(std::string text)
{
}
void TestMap1::onPlayerLeft(BWAPI::Player player)
{
}
void TestMap1::onNukeDetect(BWAPI::Position target)
{
}
void TestMap1::onUnitCreate(BWAPI::Unit unit)
{
}
void TestMap1::onUnitDestroy(BWAPI::Unit unit)
{
}
void TestMap1::onUnitMorph(BWAPI::Unit unit)
{
}
void TestMap1::onUnitShow(BWAPI::Unit unit)
{
}
void TestMap1::onUnitHide(BWAPI::Unit unit)
{
}
void TestMap1::onUnitRenegade(BWAPI::Unit unit)
{
}
void TestMap1::onSaveGame(std::string gameName)
{
}
