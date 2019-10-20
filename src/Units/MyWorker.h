#pragma once

#include "MyUnit.h"

class MyWorker : public MyUnit
{
public:
    explicit MyWorker(BWAPI::Unit unit);

protected:
    void typeSpecificUpdate() override;

    void resetMoveData() override;

    bool mineralWalk(const Choke *choke) override;

private:
    BWAPI::Unit mineralWalkingPatch;
    const BWEM::Area *mineralWalkingTargetArea;
    BWAPI::Position mineralWalkingStartPosition;
};
