#pragma once
#include "TestCase.h"
#include <BWAPI.h>

class MPHSTest : public TestCase
{
  public:
    MPHSTest(BWAPI::UnitType unitType);
    virtual void start();
    virtual void update();
    virtual void stop();
  private:
    int startFrame;
    int nextFrame;
    BWAPI::Unit unit;
    BWAPI::UnitType unitType;
    BWAPI::Position targetPosition;
};