#pragma once

namespace BWAPI {

class GameImpl;
class UnitImpl;
class PlayerImpl;
class BulletImpl;
class RegionImpl;
class ForceImpl;
class AIModule;

class CompatGameImpl {
public:
  CompatGameImpl(GameImpl* gameImpl);
  ~CompatGameImpl();
  
  AIModule* wrapAIModule(AIModule*);
  
  void** vftable;
  GameImpl* gameImpl;
  
  void*(*LoadLibrary)(const char* name);
  void*(*GetProcAddress)(void* module, const char* name);
};

class CompatUnitImpl {
public:
  CompatUnitImpl(UnitImpl* impl);
  ~CompatUnitImpl();
  
  void** vftable;
  UnitImpl* impl;
};

class CompatPlayerImpl {
public:
  CompatPlayerImpl(PlayerImpl* impl);
  ~CompatPlayerImpl();
  
  void** vftable;
  PlayerImpl* impl;
};

class CompatBulletImpl {
public:
  CompatBulletImpl(BulletImpl* impl);
  ~CompatBulletImpl();
  
  void** vftable;
  BulletImpl* impl;
};

class CompatRegionImpl {
public:
  CompatRegionImpl(RegionImpl* impl);
  ~CompatRegionImpl();
  
  void** vftable;
  RegionImpl* impl;
};

class CompatForceImpl {
public:
  CompatForceImpl(ForceImpl* impl);
  ~CompatForceImpl();
  
  void** vftable;
  ForceImpl* impl;
};

}

