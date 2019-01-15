#include "General.h"
#include "Units.h"

namespace General
{
    void issueOrders()
    {
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (!unit->isCompleted()) continue;
            if (unit->getType().isBuilding()) continue;
            if (unit->getType().isWorker()) continue;

            Units::getMine(unit).move(BWAPI::Position(BWAPI::TilePosition(BWAPI::Broodwar->mapWidth() / 2, BWAPI::Broodwar->mapHeight() / 2)));
        }
    }
}
