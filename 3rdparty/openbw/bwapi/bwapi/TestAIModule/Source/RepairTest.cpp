#include "RepairTest.h"
#include "BWAssert.h"
using namespace std;
using namespace BWAPI;
RepairTest::RepairTest(UnitType unitType, UnitType targetType) : unitType(unitType),
                                                                 unit(NULL),
                                                                 startFrame(-1),
                                                                 nextFrame(-1),
                                                                 targetType(targetType),
                                                                 target(NULL)
{
  fail = false;
  running = false;
}
void RepairTest::start()
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

  int targetCount = Broodwar->self()->completedUnitCount(targetType);
  BWAssertF(targetCount>=1,{fail=true;return;});
  for (Unit u : Broodwar->self()->getUnits())
    if (u->getType()==targetType && u->getHitPoints()<targetType.maxHitPoints())
      target = u;
  BWAssertF(target!=NULL,{fail=true;return;});
  BWAssertF(target->exists(),{fail=true;return;});
  BWAssertF(unit->isIdle()==true,{fail=true;return;});
  BWAssertF(unit->repair(target),{Broodwar->printf("%s",Broodwar->getLastError().c_str());fail=true;return;});
  BWAssertF(unit->getOrder()==Orders::Repair,{Broodwar->printf("%s",unit->getOrder().c_str());fail=true;return;});
  BWAssertF(unit->getTarget()==target,{fail=true;return;});
  startFrame = Broodwar->getFrameCount();
  nextFrame = Broodwar->getFrameCount();
  stopped = false;
}
void RepairTest::update()
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

  if (target->getHitPoints()==targetType.maxHitPoints() && !stopped)
  {
    stopped=true;
    startFrame=thisFrame;
  }
  if (!stopped)
  {
    BWAssertF(unit->getOrder()==Orders::Repair,{Broodwar->printf("%s",unit->getOrder().c_str());fail=true;return;});
    BWAssertF(unit->getTarget()==target || unit->getOrderTarget()==target,{fail=true;return;});
  }
  else
  {
    if (thisFrame>startFrame+20)
    {
      BWAssertF(unit->isIdle()==true,{fail=true;return;});
      BWAssertF(unit->isRepairing()==false,{fail=true;return;});
    }
    if (thisFrame>startFrame+60)
    {
      running = false;
    }
  }
}

void RepairTest::stop()
{
}
