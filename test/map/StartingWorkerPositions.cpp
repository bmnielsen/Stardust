#include <utility>

#include "BWTest.h"
#include "DoNothingModule.h"
#include "Map.h"
#include "Units.h"

TEST(StartingWorkerPositions, ValidateAll)
{
    bool anyError = false;
    Maps::RunOnEachStartLocation(Maps::Get(), [&anyError](BWTest test)
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

        test.onFrameMine = [&anyError]()
        {
            if (BWAPI::Broodwar->getFrameCount() != 2) return;

            std::cout << "Start location: " << BWAPI::Broodwar->self()->getStartLocation() << std::endl;

            auto expectedWorkerPositions = Map::mapSpecificOverride()->startingWorkerPositions(BWAPI::Broodwar->self()->getStartLocation());
            if (expectedWorkerPositions.size() != 4)
            {
                std::cout << "Expected worker positions is not size 4" << std::endl;
                anyError = true;
                return;
            }

            bool error = false;
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
                    error = true;
                }
            }

            for (auto remaining : expectedWorkerPositions)
            {
                std::cout << "ERROR: Did not find worker at " << remaining << std::endl;
                error = true;
            }

            anyError = anyError || error;

            if (!error)
            {
                // Check that the units are in the right order
                auto visibleUnits = BWAPI::Broodwar->getVisibleUnits();
                std::vector<BWAPI::Position> orderFound;
                for (auto &unit : visibleUnits)
                {
                    if (!unit->getType().isWorker()) continue;
                    if (unit->getPlayer() != BWAPI::Broodwar->self()) continue;
                    orderFound.push_back(unit->getPosition());
                }

                // Order should be the reverse, since the visible units list has new units first
                if (orderFound.size() != 4)
                {
                    std::cout << "Did not find 4 workers in visible units list" << std::endl;
                    anyError = true;
                    return;
                }

                expectedWorkerPositions = Map::mapSpecificOverride()->startingWorkerPositions(BWAPI::Broodwar->self()->getStartLocation());
                if (expectedWorkerPositions[0] != orderFound[3]
                    || expectedWorkerPositions[1] != orderFound[2]
                    || expectedWorkerPositions[2] != orderFound[1]
                    || expectedWorkerPositions[3] != orderFound[0])
                {
                    std::cout << "Mismatch between our list and positions in visible list" << std::endl;
                    std::cout << "Our list: "
                              << expectedWorkerPositions[0] << ", "
                              << expectedWorkerPositions[1] << ", "
                              << expectedWorkerPositions[2] << ", "
                              << expectedWorkerPositions[3] << std::endl;
                    std::cout << "Visible units list: "
                              << orderFound[3] << ", "
                              << orderFound[2] << ", "
                              << orderFound[1] << ", "
                              << orderFound[0] << std::endl;
                    anyError = true;
                }
            }
        };
        test.run();
    });

    EXPECT_EQ(false, anyError) << "An error occurred on at least one start position";
}
