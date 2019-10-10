#include "TestModule.h"
using namespace std;
using namespace BWAPI;
TestModule::TestModule() : currentTestCase(NULL), lastEndFrame(-10)
{
}

TestModule::~TestModule()
{
  for(std::list<TestCase*>::iterator i=testCases.begin();i!=testCases.end();++i)
  {
    delete *i;
  }
  testCases.clear();
}

void TestModule::onFrame()
{
  if (Broodwar->isPaused()) return;
  if (Broodwar->getFrameCount()==0) return;
  Broodwar->drawTextScreen(0,0,"FPS: %d",Broodwar->getFPS());
  Broodwar->drawTextScreen(0,20,"Assert success count: %d",assert_success_count);
  if (assert_fail_count==0)
    Broodwar->drawTextScreen(0,40,"Assert failed count: %d",assert_fail_count);
  else
    Broodwar->drawTextScreen(0,40,"\x08 Assert failed count: %d",assert_fail_count);
  for (Unit u : Broodwar->getAllUnits())
  {
    Broodwar->drawTextMap(u->getPosition().x,u->getPosition().y-16,"%s",u->getType().c_str());
    Broodwar->drawTextMap(u->getPosition(),"%s",u->getOrder().c_str());
  }
  runTestCases();
}
void TestModule::onUnitCreate(Unit unit)
{
  if (unit!=NULL)
    Broodwar->printf("A %s [%p] has been created",unit->getType().c_str(),unit);
  else
    Broodwar->printf("A %p has been created",unit);
}
void TestModule::onUnitDestroy(Unit unit)
{
  if (unit!=NULL)
    Broodwar->printf("A %s [%p] has been destroyed",unit->getType().c_str(),unit);
  else
    Broodwar->printf("A %p has been destroyed",unit);
}
void TestModule::runTestCases()
{
  if (currentTestCase==NULL && testCases.empty()==false && Broodwar->getFrameCount()>lastEndFrame+15)
  {
    currentTestCase = *testCases.begin();
    currentTestCase->start();
  }
  if (currentTestCase!=NULL)
  {
    if (currentTestCase->isRunning())
      currentTestCase->update();
    else
    {
      currentTestCase->stop();
      delete currentTestCase;
      currentTestCase = NULL;
      testCases.erase(testCases.begin());
      lastEndFrame = Broodwar->getFrameCount();
    }
  }
}

void TestModule::addTestCase(TestCase* testCase)
{
  testCases.push_back(testCase);
}

TestCase* TestModule::getCurrentTestCase() const
{
  return currentTestCase;
}