#include <utility>

#include "BWTest.h"
#include "DoNothingModule.h"
#include "Map.h"
#include "Units.h"

TEST(StartingWorkerPositions, ValidateAll)
{
    Maps::RunOnEachStartLocation(Maps::Get(), [](BWTest test)
    {
        test.myModule = []()
        {
            return new DoNothingModule();
        };
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.opponentRace = BWAPI::Races::Random;
        test.frameLimit = 10;
        test.expectWin = false;
        test.writeReplay = false;

        test.onStartMine = []()
        {
            Units::initialize();
            Map::initialize();
        };

        test.onFrameMine = []()
        {
            if (BWAPI::Broodwar->getFrameCount() != 2) return;

            std::cout << "Start location: " << BWAPI::Broodwar->self()->getStartLocation() << std::endl;

            auto expectedWorkerPositions = Map::mapSpecificOverride()->startingWorkerPositions(BWAPI::Broodwar->self()->getStartLocation());

            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (!unit->getType().isWorker()) continue;

                bool found = false;
                for (auto it = expectedWorkerPositions.begin(); it != expectedWorkerPositions.end(); )
                {
                    if (*it == unit->getPosition())
                    {
                        expectedWorkerPositions.erase(it);
                        found = true;
                        break;
                    }
                    it++;
                }
                if (!found)
                {
                    std::cout << "ERROR: Worker at unexpected position " << unit->getPosition() << std::endl;
                }
            }

            for (auto remaining : expectedWorkerPositions)
            {
                std::cout << "ERROR: Did not find worker at " << remaining << std::endl;
            }
        };
        test.run();
    });
}
