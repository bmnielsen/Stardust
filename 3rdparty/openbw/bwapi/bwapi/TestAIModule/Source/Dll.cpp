#include <BWAPI.h>
#ifdef _WIN32
#include <Windows.h>

#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

#include "TestMap1.h"
#include "TerranTest.h"
#include "ProtossTest.h"
#include "ZergTest.h"
#include "InterceptorTest.h"
#include "ExistenceTest.h"
#include "MapTest.h"
#include "EventTest.h"
#include "MicroTest.h"
#include "DefaultTestModule.h"

extern "C" DLLEXPORT void gameInit(BWAPI::Game* game) { BWAPI::BroodwarPtr = game; }
#ifdef _WIN32
BOOL APIENTRY DllMain( HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved )
{
  switch (ul_reason_for_call)
  {
  case DLL_PROCESS_ATTACH:
    break;
  case DLL_PROCESS_DETACH:
    break;
  }

  return TRUE;
}
#endif

extern "C" DLLEXPORT BWAPI::AIModule* newAIModule()
{
  if ( BWAPI::Broodwar->getGameType() == BWAPI::GameTypes::Use_Map_Settings )
  {
    std::string map = BWAPI::Broodwar->mapFileName();
    if ( map == "testmap1.scm" )
      return new TestMap1();
    if ( map == "TerranTest.scm" )
      return new TerranTest();
    if ( map == "ProtossTest.scm" )
      return new ProtossTest();
    if ( map == "ZergTest.scm" )
      return new ZergTest();
    if ( map == "InterceptorTest.scm" )
      return new InterceptorTest();
    if ( map == "ExistenceTest.scx" )
      return new ExistenceTest();
    if ( map == "MapTest.scx" )
      return new MapTest();
    if ( map == "EventTest.scm" )
      return new EventTest();
    if ( map == "MicroTest.scm" )
      return new MicroTest();
  }
  return new DefaultTestModule();
}
