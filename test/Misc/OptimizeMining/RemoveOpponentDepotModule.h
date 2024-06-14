#pragma once

#include "DoNothingModule.h"
#include "Geo.h"

class RemoveOpponentDepotModule : public DoNothingModule
{
public:
    void onFrame() override
    {
        for (auto worker : BWAPI::Broodwar->self()->getUnits())
        {
            if (worker->getType().isWorker()) BWAPI::Broodwar->killUnit(worker);
        }

        if (BWAPI::Broodwar->getFrameCount() == 10)
        {
            for (auto depot : BWAPI::Broodwar->self()->getUnits())
            {
                if (!depot->getType().isResourceDepot())
                {
                    continue;
                }

                int totalX = 0;
                int totalY = 0;
                int count = 0;
                for (auto mineral : BWAPI::Broodwar->getNeutralUnits())
                {
                    if (!mineral->getType().isMineralField()) continue;
                    if (depot->getDistance(mineral) > 320) continue;
                    totalX += mineral->getPosition().x;
                    totalY += mineral->getPosition().y;
                    count++;
                }

                auto diffX = (totalX / count) - depot->getPosition().x;
                auto diffY = (totalY / count) - depot->getPosition().y;
                BWAPI::TilePosition pylon;
                if (diffX > 0 && diffY > 0)
                {
                    pylon = depot->getTilePosition() + BWAPI::TilePosition(0, 3);
                }
                else if (diffX > 0 && diffY < 0)
                {
                    pylon = depot->getTilePosition() + BWAPI::TilePosition(-2, 0);
                }
                else if (diffX < 0 && diffY < 0)
                {
                    pylon = depot->getTilePosition() + BWAPI::TilePosition(4, 1);
                }
                else
                {
                    pylon = depot->getTilePosition() + BWAPI::TilePosition(2, -2);
                }

                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                            BWAPI::UnitTypes::Protoss_Pylon,
                                            Geo::CenterOfUnit(pylon, BWAPI::UnitTypes::Protoss_Pylon));
            }
        }

        if (BWAPI::Broodwar->getFrameCount() == 20)
        {
            for (auto depot : BWAPI::Broodwar->self()->getUnits())
            {
                if (depot->getType().isResourceDepot()) BWAPI::Broodwar->killUnit(depot);
            }
        }
    }
};