#include "InterceptorTest.h"
using namespace std;
using namespace BWAPI;
void InterceptorTest::onStart()
{
  BWAssert(Broodwar->isMultiplayer()==false);
  BWAssert(Broodwar->isReplay()==false);
  Broodwar->enableFlag(Flag::UserInput);
  Broodwar->sendText("show me the money");
  int correctCarrierCount = 2;
  int correctInterceptorCount = 8;
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Protoss_Carrier)==correctCarrierCount);
  Broodwar->printf("interceptor count = %d",Broodwar->self()->completedUnitCount(UnitTypes::Protoss_Interceptor));
  BWAssert(Broodwar->self()->completedUnitCount(UnitTypes::Protoss_Interceptor)==correctInterceptorCount);
  int carrierCount=0;
  int interceptorCount=0;
  for (Unit u : Broodwar->self()->getUnits())
  {
    if (u->getType()==UnitTypes::Protoss_Carrier)
      carrierCount++;
    if (u->getType()==UnitTypes::Protoss_Interceptor)
      interceptorCount++;
  }
  BWAssert(carrierCount==correctCarrierCount);
  BWAssert(interceptorCount==correctInterceptorCount);//fails because we cannot see interceptors until the first time they leave the carrier.
  for (Unit u : Broodwar->self()->getUnits())
  {
    if (u->getType()==UnitTypes::Protoss_Carrier)
    {
      BWAssert(u->getInterceptorCount()==4);
    }
  }
}
void InterceptorTest::onFrame()
{
  for (Unit u : Broodwar->self()->getUnits())
  {
    if (u->getType()==UnitTypes::Protoss_Interceptor)
      Broodwar->drawTextMap(u->getPosition(),"isLoaded = %d",u->isLoaded());
  }
  for (Unit u : Broodwar->self()->getUnits())
  {
    if (!u->isLoaded())
      Broodwar->drawTextMap(u->getPosition().x,u->getPosition().y-20,"loaded unit count = %d",u->getLoadedUnits().size());
  }
  for (Unit u : Broodwar->self()->getUnits())
  {
    if (u->getType()==UnitTypes::Protoss_Carrier)
    {
      Unitset interceptors = u->getInterceptors();
      for (Unit i : interceptors)
      {
        Unit c=i->getCarrier();
        Broodwar->drawLineMap(i->getPosition(),c->getPosition(),Colors::White);
      }
    }
  }
}