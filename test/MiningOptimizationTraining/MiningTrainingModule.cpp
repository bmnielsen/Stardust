#include "MiningTrainingModule.h"

#include "Units.h"
#include "Map.h"
#include "BuildingPlacement.h"

void MiningTrainingModule::onStart()
{
    InstrumentedDoNothingModule::onStart();

    // Initialize the minimum needed to have bases, start blocks and cannon placements available
    Units::initialize();
    Map::initialize();
    BuildingPlacement::initialize();

    Log::Get() << "Initialized mining training on " << BWAPI::Broodwar->mapFileName() << " (" << BWAPI::Broodwar->mapHash() << ")";
    CherryVis::setBoardValue("dummy", "yes"); // just setting it so we write the cvis file and don't get errors in the frontend
}

void MiningTrainingModule::onFrame()
{
    // Ensure all mineral patches keep enough minerals
    if (currentFrame % 500 == 42)
    {
        for (auto unit : BWAPI::Broodwar->getNeutralUnits())
        {
            if (!unit->getType().isMineralField()) continue;
            if (unit->getResources() < 100) unit->setResources(1500);
        }
    }
}
