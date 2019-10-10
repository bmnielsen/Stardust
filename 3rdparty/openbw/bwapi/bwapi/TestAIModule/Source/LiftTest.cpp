#include "LiftTest.h"
#include "BWAssert.h"
using namespace std;
using namespace BWAPI;
LiftTest::LiftTest(UnitType unitType) : unitType(unitType),
                                        unit(NULL),
                                        startFrame(-1),
                                        nextFrame(-1),
                                        completedLift(false)
{
  fail = false;
  running = false;
}
void LiftTest::start()
{
  if (fail) return;
  running = true;

  int userCount = Broodwar->self()->completedUnitCount(unitType);
  BWAssertF(userCount>=1,{fail=true;return;});
  for (Unit u : Broodwar->self()->getUnits())
    if (u->getType()==unitType)
      unit = u;

  BWAssertF(unit!=NULL,{fail=true;return;});
  BWAssertF(unit->exists(),{fail=true;return;});
  BWAssertF(unit->isLifted()==false,{fail=true;return;});
  BWAssertF(unit->isIdle()==true,{fail=true;return;});
  BWAssertF(unit->isTraining()==false,{fail=true;return;});
  BWAssertF(unit->isResearching()==false,{fail=true;return;});
  BWAssertF(unit->isUpgrading()==false,{fail=true;return;});
  BWAssertF(unit->isConstructing()==false,{fail=true;return;});
  originalTilePosition = unit->getTilePosition();
  unit->lift();
  BWAssertF(unit->getOrder()==Orders::BuildingLiftOff,{fail=true;return;});
  startFrame = Broodwar->getFrameCount();
  nextFrame = Broodwar->getFrameCount();

}
void LiftTest::update()
{
  if (running == false) return;
  if (fail)
  {
    running = false;
    return;
  }
  int thisFrame = Broodwar->getFrameCount();
  BWAssert(thisFrame==nextFrame);
  nextFrame++;
  Broodwar->setScreenPosition(unit->getPosition() - Position(320,240));

  if (completedLift==false)
  {
    if (unit->getOrder()!=Orders::BuildingLiftOff && unit->getOrder()!=Orders::LiftingOff && thisFrame>startFrame+300)
    {
      BWAssertF(unit->isLifted()==true,{fail=true;return;});
      completedLift=true;
      unit->land(originalTilePosition);
      BWAssertF(unit->getOrder() == Orders::BuildingLand, { fail = true; log("originalTilePosition=(%d,%d)", originalTilePosition.x, originalTilePosition.y); return; });
      startFrame=thisFrame;
    }
    else
    {
      if (thisFrame>startFrame+600)
        fail = true;
    }
  }
  else
  {
    if (unit->getOrder()!=Orders::BuildingLand && thisFrame>startFrame+300)
    {
      BWAssertF(unit->isLifted()==false,{fail=true;return;});
      running = false;
    }
    else
    {
      if (thisFrame>startFrame+600)
        fail = true;
    }
  }
}

void LiftTest::stop()
{
}
