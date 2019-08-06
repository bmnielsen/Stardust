#pragma once

#include "MyUnit.h"

class MyWorker : public MyUnit
{
public:
    MyWorker(BWAPI::Unit unit);

protected:
    void typeSpecificUpdate();
    void resetMoveData();
    bool mineralWalk(const Choke * choke = nullptr);

private:
    BWAPI::Unit                         mineralWalkingPatch;
    const BWEM::Area*                   mineralWalkingTargetArea;
    BWAPI::Position                     mineralWalkingStartPosition;
};
