#pragma once
#include "TestCase.h"
#include <BWAPI.h>

class UseTechTest : public TestCase
{
  public:
    UseTechTest(BWAPI::TechType techType);
    virtual void start();
    virtual void update();
    virtual void stop();
  private:
    void checkPosition();
    void useTech();
    BWAPI::TechType techType;
    BWAPI::UnitType userType;
    int startFrame;
    int nextFrame;
    BWAPI::Unit user;
    BWAPI::Position targetPosition;
    BWAPI::Unit targetUnit;
    bool isInPosition;
    bool usedTech;
    bool testSucceeded;
    BWAPI::Position startPosition;
    BWAPI::UnitType targetType;
    int currentEnergy;
};