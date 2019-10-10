#include "CancelTrainTest.h"
#include "BWAssert.h"
using namespace std;
using namespace BWAPI;
CancelTrainTest::CancelTrainTest(BWAPI::UnitType unitType1, BWAPI::UnitType unitType2, BWAPI::UnitType unitType3)
                                               : unitType1(unitType1),
                                                 unitType2(unitType2),
                                                 unitType3(unitType3),
                                                 producerType(unitType1.whatBuilds().first),
                                                 producer(NULL),
                                                 startFrame(-1),
                                                 nextFrame(-1),
                                                 correctMineralCount(0),
                                                 correctGasCount(0),
                                                 correctSupplyUsedCount(0),
                                                 originalMineralCount(0),
                                                 originalGasCount(0),
                                                 originalSupplyUsedCount(0),
                                                 originalAllUnit1Count(0),
                                                 originalAllUnit2Count(0),
                                                 originalAllUnit3Count(0),
                                                 state(Init)
{
  fail = false;
  running = false;
  BWAssertF(producerType==unitType2.whatBuilds().first,{fail=true;return;});
  BWAssertF(producerType==unitType3.whatBuilds().first,{fail=true;return;});
  BWAssertF(producerType!=UnitTypes::None,{fail=true;return;});
  BWAssertF(producerType!=UnitTypes::Unknown,{fail=true;return;});
}
bool CancelTrainTest::verifyTrainingQueue()
{
  BWAssertF(producer!=NULL,{fail=true;return false;});
  UnitType::list actualList = producer->getTrainingQueue();
  BWAssertF(correctTrainingQueue.size() == actualList.size(),{log("Error training queue size %d != %d",correctTrainingQueue.size(),actualList.size());fail=true;return false;});
  BWAssertF(correctTrainingQueue == actualList,{fail=true;return false;});
  return true;
}
void CancelTrainTest::start()
{
  if (fail) return;
  running = true;

  int producerCount = Broodwar->self()->completedUnitCount(producerType);
  BWAssertF(producerCount>=1,{fail=true;return;});
  for (Unit u : Broodwar->self()->getUnits())
  {
    if (u->getType()==producerType)
    {
      producer = u;
      break;
    }
  }
  BWAssertF(producer!=NULL,{fail=true;return;});

  originalMineralCount = Broodwar->self()->minerals();
  originalGasCount = Broodwar->self()->gas();
  originalSupplyUsedCount = Broodwar->self()->supplyUsed();
  originalAllUnit1Count = Broodwar->self()->allUnitCount(unitType1);
  originalAllUnit2Count = Broodwar->self()->allUnitCount(unitType2);
  originalAllUnit3Count = Broodwar->self()->allUnitCount(unitType3);
  correctMineralCount = Broodwar->self()->minerals();
  correctGasCount = Broodwar->self()->gas();
  correctSupplyUsedCount = Broodwar->self()->supplyUsed();

  BWAssertF(producer->isTraining()==false,{fail=true;return;});
  BWAssertF(producer->isConstructing()==false,{fail=true;return;});
  BWAssertF(producer->isIdle()==true,{fail=true;return;});
  BWAssertF(producer->isLifted()==false,{fail=true;return;});
  BWAssertF(producer->getTrainingQueue().empty()==true,{fail=true;return;});
  BWAssertF(producer->getRemainingTrainTime() == 0,{fail=true;return;});
  BWAssertF(Broodwar->self()->allUnitCount(unitType1) == originalAllUnit1Count,{fail=true;return;});
  BWAssertF(Broodwar->self()->allUnitCount(unitType2) == originalAllUnit2Count,{fail=true;return;});
  BWAssertF(Broodwar->self()->allUnitCount(unitType3) == originalAllUnit3Count,{fail=true;return;});

  producer->train(unitType1);
  correctMineralCount = correctMineralCount - unitType1.mineralPrice();
  correctGasCount = correctGasCount - unitType1.gasPrice();
  correctSupplyUsedCount = correctSupplyUsedCount + unitType1.supplyRequired();
  correctTrainingQueue.push_back(unitType1);

  BWAssertF(verifyTrainingQueue(),{fail=true;return;});
  BWAssertF(producer->isTraining()==true,{fail=true;return;});
  BWAssertF(producer->isConstructing()==false,{fail=true;return;});
  BWAssertF(producer->isIdle()==false,{fail=true;return;});
  BWAssertF(producer->isLifted()==false,{fail=true;return;});
  BWAssertF(producer->getTrainingQueue().size()==1,{fail=true;return;});
  BWAssertF(producer->getRemainingTrainTime() == unitType1.buildTime(),{fail=true;return;});
  BWAssertF(Broodwar->self()->minerals() == correctMineralCount,{Broodwar->printf("%d != %d",Broodwar->self()->minerals(), correctMineralCount);fail=true;return;});
  BWAssertF(Broodwar->self()->gas() == correctGasCount,{fail=true;return;});
  BWAssertF(Broodwar->self()->supplyUsed() == correctSupplyUsedCount,{fail=true;return;});

  producer->train(unitType2);
  correctMineralCount = correctMineralCount - unitType2.mineralPrice();
  correctGasCount = correctGasCount - unitType2.gasPrice();
  correctTrainingQueue.push_back(unitType2);

  BWAssertF(verifyTrainingQueue(),{fail=true;return;});
  BWAssertF(producer->isTraining()==true,{fail=true;return;});
  BWAssertF(producer->isConstructing()==false,{fail=true;return;});
  BWAssertF(producer->isIdle()==false,{fail=true;return;});
  BWAssertF(producer->isLifted()==false,{fail=true;return;});
  BWAssertF(producer->getTrainingQueue().size()==2,{fail=true;return;});
  BWAssertF(producer->getRemainingTrainTime() == unitType1.buildTime(),{fail=true;return;});
  BWAssertF(Broodwar->self()->minerals() == correctMineralCount,{fail=true;return;});
  BWAssertF(Broodwar->self()->gas() == correctGasCount,{fail=true;return;});
  BWAssertF(Broodwar->self()->supplyUsed() == correctSupplyUsedCount,{fail=true;return;});

  producer->train(unitType3);
  correctMineralCount = correctMineralCount - unitType3.mineralPrice();
  correctGasCount = correctGasCount - unitType3.gasPrice();
  correctTrainingQueue.push_back(unitType3);

  BWAssertF(verifyTrainingQueue(),{fail=true;return;});
  BWAssertF(producer->isTraining()==true,{fail=true;return;});
  BWAssertF(producer->isConstructing()==false,{fail=true;return;});
  BWAssertF(producer->isIdle()==false,{fail=true;return;});
  BWAssertF(producer->isLifted()==false,{fail=true;return;});
  BWAssertF(producer->getTrainingQueue().size()==3,{fail=true;return;});
  BWAssertF(producer->getRemainingTrainTime() == unitType1.buildTime(),{fail=true;return;});
  BWAssertF(Broodwar->self()->minerals() == correctMineralCount,{fail=true;return;});
  BWAssertF(Broodwar->self()->gas() == correctGasCount,{fail=true;return;});
  BWAssertF(Broodwar->self()->supplyUsed() == correctSupplyUsedCount,{fail=true;return;});

  startFrame = Broodwar->getFrameCount();
  nextFrame = startFrame;
  state = Start;

}
void CancelTrainTest::update()
{
  if (running == false) return;
  if (fail)
  {
    running = false;
    return;
  }
  int thisFrame = Broodwar->getFrameCount();
  BWAssert(thisFrame==nextFrame);
  BWAssertF(producer!=NULL,{fail=true;return;});
  nextFrame++;
  Broodwar->setScreenPosition(producer->getPosition() - Position(320,240));
  if (state==Start)
  {
    BWAssertF(producer->isTraining()==true,{fail=true;return;});
    BWAssertF(producer->isConstructing()==false,{fail=true;return;});
    BWAssertF(producer->isIdle()==false,{fail=true;return;});
    BWAssertF(producer->isLifted()==false,{fail=true;return;});
    BWAssertF(verifyTrainingQueue(),{log("Error training queue failed at %u frame %d",__LINE__,thisFrame-startFrame);fail=true;return;});
    BWAssertF(Broodwar->self()->minerals() == correctMineralCount,{fail=true;return;});
    BWAssertF(Broodwar->self()->gas() == correctGasCount,{fail=true;return;});
    BWAssertF(Broodwar->self()->supplyUsed() == correctSupplyUsedCount,{log("Error supply used %d != %d",Broodwar->self()->supplyUsed(),correctSupplyUsedCount);fail=true;return;});
    BWAssertF(Broodwar->self()->allUnitCount(unitType2) == originalAllUnit2Count,{fail=true;return;});
    BWAssertF(Broodwar->self()->allUnitCount(unitType3) == originalAllUnit3Count,{fail=true;return;});
    if (thisFrame>=startFrame+15)
    {
      BWAssertF(producer->cancelTrain(0),{Broodwar->printf("%s",Broodwar->getLastError().c_str());fail=true;return;});
      correctTrainingQueue.pop_front();
      BWAssertF(verifyTrainingQueue(),{log("Error training queue failed at %u frame %d",__LINE__,thisFrame-startFrame-150);fail=true;return;});
      correctMineralCount = correctMineralCount + unitType1.mineralPrice();
      correctGasCount = correctGasCount + unitType1.gasPrice();
      correctSupplyUsedCount = correctSupplyUsedCount - unitType1.supplyRequired() + unitType2.supplyRequired();
      BWAssertF(Broodwar->self()->minerals() == correctMineralCount,{Broodwar->printf("%d != %d",Broodwar->self()->minerals(),correctMineralCount);fail=true;return;});
      BWAssertF(Broodwar->self()->gas() == correctGasCount,{fail=true;return;});
      BWAssertF(Broodwar->self()->supplyUsed() == correctSupplyUsedCount,{fail=true;return;});
      state = CancelledFirstSlot;
    }
  }
  else if (state == CancelledFirstSlot)
  {
    BWAssertF(producer->isTraining()==true,{fail=true;return;});
    BWAssertF(producer->isConstructing()==false,{fail=true;return;});
    BWAssertF(producer->isIdle()==false,{fail=true;return;});
    BWAssertF(producer->isLifted()==false,{fail=true;return;});
    BWAssertF(verifyTrainingQueue(),{log("Error training queue failed at %u frame %d",__LINE__,thisFrame-startFrame-150);fail=true;return;});
    BWAssertF(Broodwar->self()->minerals() == correctMineralCount,{fail=true;return;});
    BWAssertF(Broodwar->self()->gas() == correctGasCount,{fail=true;return;});
    BWAssertF(Broodwar->self()->supplyUsed() == correctSupplyUsedCount,{log("Error supply used %d: %d != %d",thisFrame-startFrame-150,Broodwar->self()->supplyUsed(),correctSupplyUsedCount);});
    if (thisFrame>=startFrame+30)
    {
      BWAssertF(producer->cancelTrain(),{Broodwar->printf("%s",Broodwar->getLastError().c_str());fail=true;return;});
      correctTrainingQueue.pop_back();
      BWAssertF(verifyTrainingQueue(),{log("Error training queue failed at %u frame %d",__LINE__,thisFrame-startFrame-30);fail=true;return;});
      correctMineralCount = correctMineralCount + unitType3.mineralPrice();
      correctGasCount = correctGasCount + unitType3.gasPrice();
      BWAssertF(Broodwar->self()->minerals() == correctMineralCount,{fail=true;return;});
      BWAssertF(Broodwar->self()->gas() == correctGasCount,{fail=true;return;});
      BWAssertF(Broodwar->self()->supplyUsed() == correctSupplyUsedCount,{fail=true;return;});
      state = CancelledLast1;
    }
  }
  else if (state == CancelledLast1)
  {
    BWAssertF(producer->isTraining()==true,{fail=true;return;});
    BWAssertF(producer->isConstructing()==false,{fail=true;return;});
    BWAssertF(producer->isIdle()==false,{fail=true;return;});
    BWAssertF(producer->isLifted()==false,{fail=true;return;});
    BWAssertF(verifyTrainingQueue(),{fail=true;return;});
    BWAssertF(Broodwar->self()->minerals() == correctMineralCount,{fail=true;return;});
    BWAssertF(Broodwar->self()->gas() == correctGasCount,{fail=true;return;});
    BWAssertF(Broodwar->self()->supplyUsed() == correctSupplyUsedCount,{fail=true;return;});
    if (thisFrame>=startFrame+45)
    {
      BWAssertF(producer->cancelTrain(),{Broodwar->printf("%s",Broodwar->getLastError().c_str());fail=true;return;});
      correctTrainingQueue.pop_back();
      BWAssertF(verifyTrainingQueue(),{log("Error training queue failed at %u frame %d",__LINE__,thisFrame-startFrame-450);fail=true;return;});
      BWAssertF(producer->getTrainingQueue().size()==0,{fail=true;return;});
      BWAssertF(producer->isTraining()==false,{fail=true;return;});
      BWAssertF(producer->isConstructing()==false,{fail=true;return;});
      BWAssertF(producer->isIdle()==true,{fail=true;return;});
      BWAssertF(producer->isLifted()==false,{fail=true;return;});
      correctMineralCount = correctMineralCount + unitType2.mineralPrice();
      correctGasCount = correctGasCount + unitType2.gasPrice();
      correctSupplyUsedCount = correctSupplyUsedCount - unitType2.supplyRequired();
      BWAssertF(Broodwar->self()->minerals() == correctMineralCount,{fail=true;return;});
      BWAssertF(Broodwar->self()->gas() == correctGasCount,{fail=true;return;});
      BWAssertF(Broodwar->self()->supplyUsed() == correctSupplyUsedCount,{fail=true;return;});
      BWAssertF(Broodwar->self()->minerals() == originalMineralCount,{fail=true;return;});
      BWAssertF(Broodwar->self()->gas() == originalGasCount,{fail=true;return;});
      BWAssertF(Broodwar->self()->supplyUsed() == originalSupplyUsedCount,{fail=true;return;});
      state = CancelledLast2;
    }
  }
  else if (state == CancelledLast2)
  {
    BWAssertF(producer!=NULL,{fail=true;return;});
    BWAssertF(producer->isConstructing()==false,{fail=true;return;});
    BWAssertF(producer->isIdle()==true,{fail=true;return;});
    BWAssertF(producer->isLifted()==false,{fail=true;return;});
    BWAssertF(producer->isTraining()==false,{fail=true;return;});
    BWAssertF(producer->getTrainingQueue().empty()==true,{fail=true;return;});
    BWAssertF(Broodwar->self()->minerals() == originalMineralCount,{fail=true;return;});
    BWAssertF(Broodwar->self()->gas() == originalGasCount,{fail=true;return;});
    BWAssertF(Broodwar->self()->supplyUsed() == originalSupplyUsedCount,{fail=true;return;});
    if (thisFrame>startFrame+40+rand()%4)
    {
      running=false;
    }
  }
}

void CancelTrainTest::stop()
{
  if (fail == true) return;
  BWAssertF(producer!=NULL,{fail=true;return;});
  BWAssertF(producer->isConstructing()==false,{fail=true;return;});
  BWAssertF(producer->isIdle()==true,{fail=true;return;});
  BWAssertF(producer->isLifted()==false,{fail=true;return;});
  BWAssertF(producer->isTraining()==false,{fail=true;return;});
  BWAssertF(producer->getTrainingQueue().empty()==true,{fail=true;return;});
  BWAssertF(Broodwar->self()->minerals() == originalMineralCount,{fail=true;return;});
  BWAssertF(Broodwar->self()->gas() == originalGasCount,{fail=true;return;});
  BWAssertF(Broodwar->self()->supplyUsed() == originalSupplyUsedCount,{fail=true;return;});
  BWAssertF(Broodwar->self()->allUnitCount(unitType1) == originalAllUnit1Count,{fail=true;return;});
  BWAssertF(Broodwar->self()->allUnitCount(unitType2) == originalAllUnit2Count,{fail=true;return;});
  BWAssertF(Broodwar->self()->allUnitCount(unitType3) == originalAllUnit3Count,{fail=true;return;});
}
