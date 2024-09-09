#pragma once

#include "DoNothingModule.h"
#include "Geo.h"

class ClearOpponentUnitsModule : public DoNothingModule
{
public:
    void onFrame() override
    {
        // Kill all workers and lift the depot
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType().isWorker())
            {
                BWAPI::Broodwar->killUnit(unit);
            }
            if (unit->getType().isResourceDepot())
            {
                if (!unit->isLifted())
                {
                    unit->lift();
                }
                else if (unit->getOrder() != BWAPI::Orders::Move)
                {
                    unit->move(BWAPI::Position(0, 0));
                }

                if (unit->getHitPoints() < 100)
                {
                    unit->setHitPoints(1500);
                }
            }
        }
    }
};