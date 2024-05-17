#include "BWTest.h"
#include "DoNothingModule.h"

#include "Map.h"
#include "Units.h"
#include "Workers.h"

TEST(Plasma, FindsEnemyBase)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Plasma");
    test.randomSeed = 42;
    test.frameLimit = 5000;
    test.expectWin = false;

    test.onEndMine = [](bool won)
    {
        EXPECT_TRUE(Map::getEnemyStartingMain() != nullptr);
    };

    test.run();
}

TEST(Plasma, WalkAllChokes)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Plasma");
    test.randomSeed = 42;
    test.frameLimit = 10000;
    test.expectWin = false;

    std::vector<std::pair<BWAPI::TilePosition, BWAPI::TilePosition>> chokes = {
            std::make_pair(BWAPI::TilePosition(10, 73), BWAPI::TilePosition(11, 54)),
            std::make_pair(BWAPI::TilePosition(30, 104), BWAPI::TilePosition(44, 94)),
            std::make_pair(BWAPI::TilePosition(81, 107), BWAPI::TilePosition(62, 119)),
            std::make_pair(BWAPI::TilePosition(64, 64), BWAPI::TilePosition(45, 64)),
            std::make_pair(BWAPI::TilePosition(63, 11), BWAPI::TilePosition(81, 22)),
            std::make_pair(BWAPI::TilePosition(29, 25), BWAPI::TilePosition(42, 33)),
    };

    auto current = chokes.begin();

    // 0 = initial state
    // 1 = moving towards first position
    // 2 = moving towards second position
    // 3 = moving back to first position
    int chokeState = 0;
    int currentMoveStarted = 0;
    MyUnit worker;

    test.onFrameMine = [&]()
    {
        if (current == chokes.end())
        {
            BWAPI::Broodwar->leaveGame();
            return;
        }

        if (BWAPI::Broodwar->getFrameCount() - currentMoveStarted > 2000)
        {
            std::cout << "Fail: took more than 2000 frames to get to the next target position" << std::endl;
            EXPECT_TRUE(false);
            BWAPI::Broodwar->leaveGame();
            return;
        }

        if (!worker)
        {
            for (const auto &unit : Units::allMine())
            {
                if (unit->type == BWAPI::UnitTypes::Protoss_Probe)
                {
                    Log::Get() << "Reserved worker " << *unit;
                    Workers::reserveWorker(unit);
                    worker = unit;
                    break;
                }
            }
            if (!worker) return;
        }

        // If the worker has already done something this frame, we can't issue another order to it so wait for the next one
        if (worker->bwapiUnit->getLastCommandFrame() > (BWAPI::Broodwar->getFrameCount() - 2)) return;

        BWAPI::Position currentTarget;
        if (chokeState == 2)
        {
            currentTarget = BWAPI::Position(current->second) + BWAPI::Position(16, 16);
        }
        else
        {
            currentTarget = BWAPI::Position(current->first) + BWAPI::Position(16, 16);
        }

        if (chokeState == 0)
        {
            worker->moveTo(currentTarget);
            worker->issueMoveOrders();
            currentMoveStarted = BWAPI::Broodwar->getFrameCount();
            chokeState++;
            Log::Get() << "Started move to new choke @ " << BWAPI::TilePosition(currentTarget);
            return;
        }

        int dist = worker->getDistance(currentTarget);
        if (dist < 32)
        {
            chokeState++;
            if (chokeState > 3)
            {
                chokeState = 0;
                current++;
            }
            else
            {
                BWAPI::Position nextTarget;
                if (chokeState == 2)
                {
                    nextTarget = BWAPI::Position(current->second) + BWAPI::Position(16, 16);
                }
                else
                {
                    nextTarget = BWAPI::Position(current->first) + BWAPI::Position(16, 16);
                }
                worker->moveTo(nextTarget);
                worker->issueMoveOrders();
                Log::Get() << "Started move to " << BWAPI::TilePosition(nextTarget);
                currentMoveStarted = BWAPI::Broodwar->getFrameCount();
            }
        }
    };

    test.onEndMine = [&](bool won)
    {
        EXPECT_TRUE(current == chokes.end());
    };

    test.run();
}
