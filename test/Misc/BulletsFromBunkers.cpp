#include "Bullets.h"
#include "BWTest.h"

#include "DoNothingModule.h"
#include "DoNothingStrategyEngine.h"
#include "EnemyBunker.h"
#include "InstrumentedDoNothingModule.h"
#include "Strategist.h"

#define OUTPUT_MARINE_COOLDOWNS false
#define ITERATIONS_PER_TEST 1

namespace
{
    // Module that loads marines into the nearest bunker
    // Supports configurable unloading some number of marines on a specific frame
    class BunkerModule : public DoNothingModule
    {
    public:
        explicit BunkerModule(int unloadCount = 0) : unloadCount(unloadCount) {}

    private:

        int unloadCount;

        bool initialLoadComplete = false;

        void onFrame() override
        {
            auto getUnitsOfType = [](const BWAPI::UnitType type)
            {
                std::vector<BWAPI::Unit> units;
                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType() == type)
                    {
                        units.push_back(unit);
                    }
                }

                // Sort to ensure stable behaviour between executions
                std::sort(
                    units.begin(),
                    units.end(),
                    [](const BWAPI::Unit &a, const BWAPI::Unit &b){return a->getID() < b->getID();});

                return units;
            };

            if (!initialLoadComplete)
            {
                auto bunkers = getUnitsOfType(BWAPI::UnitTypes::Terran_Bunker);
                if (bunkers.empty()) return;

                // Clear the overlord added for vision on bunker spawn locations at init time
                for (auto unit : getUnitsOfType(BWAPI::UnitTypes::Zerg_Overlord))
                {
                    BWAPI::Broodwar->killUnit(unit);
                }

                bool allLoaded = true;
                bool anyLoaded = false;
                for (auto marine : getUnitsOfType(BWAPI::UnitTypes::Terran_Marine))
                {
                    if (marine->isLoaded())
                    {
                        anyLoaded = true;
                    }
                    else
                    {
                        allLoaded = false;
                        if (marine->getLastCommand().getType() != BWAPI::UnitCommandTypes::Load)
                        {
                            int closestDist = INT_MAX;
                            BWAPI::Unit closestBunker = nullptr;
                            for (auto bunker : bunkers)
                            {
                                int dist = marine->getDistance(bunker);
                                if (dist < closestDist)
                                {
                                    closestDist = dist;
                                    closestBunker = bunker;
                                }
                            }

                            if (closestBunker)
                            {
                                marine->load(closestBunker);
                            }
                        }
                    }
                }

                initialLoadComplete = allLoaded && anyLoaded;
                return;
            }

            if (unloadCount > 0 && BWAPI::Broodwar->getFrameCount() == 330)
            {
                int toUnload = unloadCount;
                for (auto bunker : getUnitsOfType(BWAPI::UnitTypes::Terran_Bunker))
                {
                    if (toUnload <= 0) break;

                    for (auto loadedUnit : bunker->getLoadedUnits())
                    {
                        bunker->unload(loadedUnit);
                        --toUnload;
                        if (toUnload <= 0) break;
                    }
                }
            }

            if (unloadCount > 0 && BWAPI::Broodwar->getFrameCount() > 330)
            {
                for (auto marine : getUnitsOfType(BWAPI::UnitTypes::Terran_Marine))
                {
                    if (!marine->isLoaded())
                    {
                        BWAPI::Broodwar->killUnit(marine);
                    }
                }
            }

#if OUTPUT_MARINE_COOLDOWNS
            for (auto marine : getUnitsOfType(BWAPI::UnitTypes::Terran_Marine))
            {
                std::cout << BWAPI::Broodwar->getFrameCount()
                          << ": " << marine->getID()
                          << ": " << marine->getGroundWeaponCooldown()
                          << ", " << marine->getOrderTimer()
                          << ", " << marine->getOrderProcessTimer()
                          << std::endl;
            }
#endif
        }
    };

    // Stripped-down Stardust test module that just updates the state so we can verify it
    class StardustBunkerTestModule : public InstrumentedDoNothingModule
    {
        void onStart() override
        {
            InstrumentedDoNothingModule::onStart();
            Bullets::initialize();
            Units::initialize();
            Map::initialize();
            Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());
        }

        void onFrame() override
        {
            InstrumentedDoNothingModule::onFrameStart();

            Bullets::update();
            Units::update();
            Bullets::updateBunkers();
            Map::update();

            InstrumentedDoNothingModule::onFrameEnd();
        }
    };

    void run(std::vector<int> bunkersAndMarineCounts,
             bool visionAtFirstVolley = true,
             int unloadCount = 0,
             int marineRangeAtFrame = -1,
             int seedOverride = -1)
    {
        BunkerModule opponentModule(unloadCount);
        StardustBunkerTestModule myModule;

        BWTest test;
        test.opponentModule = [&]()
        {
            return &opponentModule;
        };
        test.myModule = [&]()
        {
            return &myModule;
        };
        test.opponentRace = BWAPI::Races::Terran;
        test.map = Maps::GetOne("Circuit Breaker");
        test.randomSeed = seedOverride;
        test.frameLimit = 550;
        test.expectWin = false;
        test.allowOpponentOutput = true;

        std::vector<UnitTypeAndPosition> initialOpponentUnits;
        initialOpponentUnits.emplace_back(BWAPI::UnitTypes::Zerg_Overlord, BWAPI::TilePosition(115, 28), true); // for vision
        auto addMarine = [&](int bunkerIndex, int marineIndex, BWAPI::TilePosition position)
        {
            if (bunkersAndMarineCounts[bunkerIndex - 1] < marineIndex) return;
            initialOpponentUnits.emplace_back(BWAPI::UnitTypes::Terran_Marine, position);
        };
        if (visionAtFirstVolley)
        {
            if (bunkersAndMarineCounts.size() >= 1)
            {
                initialOpponentUnits.emplace_back(BWAPI::UnitTypes::Terran_Bunker, BWAPI::TilePosition(113, 29));
                addMarine(1, 1, {112, 28});
                addMarine(1, 2, {113, 28});
                addMarine(1, 3, {114, 28});
                addMarine(1, 4, {115, 28});
            }
            if (bunkersAndMarineCounts.size() >= 2)
            {
                initialOpponentUnits.emplace_back(BWAPI::UnitTypes::Terran_Bunker, BWAPI::TilePosition(113, 31));
                addMarine(2, 1, {112, 33});
                addMarine(2, 2, {113, 33});
                addMarine(2, 3, {114, 33});
                addMarine(2, 4, {115, 33});
            }
        }
        else
        {
            if (bunkersAndMarineCounts.size() >= 1)
            {
                initialOpponentUnits.emplace_back(BWAPI::UnitTypes::Terran_Bunker, BWAPI::TilePosition(110, 23));
                addMarine(1, 1, {109, 23});
                addMarine(1, 2, {109, 22});
                addMarine(1, 3, {110, 22});
                addMarine(1, 4, {111, 22});
            }
            if (bunkersAndMarineCounts.size() >= 2)
            {
                initialOpponentUnits.emplace_back(BWAPI::UnitTypes::Terran_Bunker, BWAPI::TilePosition(113, 23));
                addMarine(2, 1, {116, 23});
                addMarine(2, 2, {116, 22});
                addMarine(2, 3, {115, 22});
                addMarine(2, 4, {114, 22});
            }
        }

        test.opponentInitialUnits = initialOpponentUnits;

        test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(100, 38)),
        };

        if (marineRangeAtFrame == 0)
        {
            test.onStartOpponent = []()
            {
                BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::U_238_Shells, 1);
            };
        }
        else if (marineRangeAtFrame > 0)
        {
            test.onFrameOpponent = [&]()
            {
                if (BWAPI::Broodwar->getFrameCount() == marineRangeAtFrame)
                {
                    BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::U_238_Shells, 1);
                }
            };
        }

        unsigned int misdetectionsOnSecondEncounter = 0;
        test.onFrameMine = [&]()
        {
            // Micro the zealot in and out of bunker range twice
            []()
            {
                BWAPI::Unit zealot = nullptr;
                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType() == BWAPI::UnitTypes::Protoss_Zealot)
                    {
                        zealot = unit;
                        break;
                    }
                }
                if (!zealot) return;

                if (BWAPI::Broodwar->getFrameCount() % 10 == 0)
                {
                    zealot->setHitPoints(BWAPI::UnitTypes::Protoss_Zealot.maxHitPoints());
                    zealot->setShields(BWAPI::UnitTypes::Protoss_Zealot.maxShields());
                }

                if (BWAPI::Broodwar->getFrameCount() == 100)
                {
                    zealot->move(BWAPI::Position(BWAPI::WalkPosition(451, 114)));
                }

                if (BWAPI::Broodwar->getFrameCount() == 240)
                {
                    zealot->move(BWAPI::Position(BWAPI::WalkPosition(418, 138)));
                }

                if (BWAPI::Broodwar->getFrameCount() == 330)
                {
                    zealot->move(BWAPI::Position(BWAPI::WalkPosition(451, 114)));
                }

                if (BWAPI::Broodwar->getFrameCount() == 425)
                {
                    zealot->move(BWAPI::Position(BWAPI::WalkPosition(418, 138)));
                }
            }();

            // Assertions
            // We ensure that we don't go over the actual number of marines in the bunkers on the first encounter,
            // and don't go under the actual number of marines in the bunkers on the second encounter for more than a few frames
            int totalMarinesDetected = 0;
            for (auto &unit : Units::allEnemyOfType(BWAPI::UnitTypes::Terran_Bunker))
            {
                auto bunker = std::dynamic_pointer_cast<EnemyBunker>(unit);
                if (bunker)
                {
                    totalMarinesDetected += bunker->loadedMarines;
                }
            }

            int initialMarinesLoaded = 0;
            for (auto count : bunkersAndMarineCounts) initialMarinesLoaded += count;

            if (BWAPI::Broodwar->getFrameCount() < 285)
            {
                EXPECT_LE(totalMarinesDetected, initialMarinesLoaded)
                    << currentFrame << ": Detected too many marines during first encounter";
            }
            else if (BWAPI::Broodwar->getFrameCount() < 330)
            {
                EXPECT_EQ(initialMarinesLoaded, totalMarinesDetected)
                    << currentFrame << ": Detected incorrect number of marines after first encounter";
            }
            else if (BWAPI::Broodwar->getFrameCount() < 460)
            {
                if (totalMarinesDetected < (initialMarinesLoaded - unloadCount))
                {
                    ++misdetectionsOnSecondEncounter;
                    if (misdetectionsOnSecondEncounter == 4)
                    {
                        EXPECT_GE(totalMarinesDetected, initialMarinesLoaded - unloadCount)
                            << currentFrame << ": Detected too few marines during second encounter for at least 4 frames";
                    }
                }
            }
            else
            {
                EXPECT_EQ(initialMarinesLoaded - unloadCount, totalMarinesDetected)
                    << currentFrame << ": Detected incorrect number of marines after second encounter";
            }
        };

        test.onStartMine = [&]()
        {
            std::ostringstream out;
            std::string sep;
            out << "run({";
            for (auto count : bunkersAndMarineCounts)
            {
                out << sep << count;
                sep = ",";
            }
            out << "},";
            if (visionAtFirstVolley)
            {
                out << "true";
            }
            else
            {
                out << "false";
            }
            out << "," << unloadCount << "," << marineRangeAtFrame << "," << test.randomSeed << ");";
            Log::Get() << out.str();

            std::ostringstream replayName;
            replayName << ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
            replayName << "_" << ::testing::UnitTest::GetInstance()->current_test_info()->name();
            replayName << "_" << out.str();
            test.replayName = replayName.str();
        };

        test.run();
    }
}

TEST(BulletsFromBunkers, OneBunkerLowGround)
{
    for (int i = 0; i < ITERATIONS_PER_TEST; ++i)
    {
        run({0});
        run({1});
        run({2});
        run({3});
        run({4});
    }
}

TEST(BulletsFromBunkers, OneBunkerWithUnloadingLowGround)
{
    for (int i = 0; i < ITERATIONS_PER_TEST; ++i)
    {
        run({1}, true, 1);
        run({2}, true, 1);
        run({2}, true, 2);
        run({3}, true, 1);
        run({3}, true, 2);
        run({3}, true, 3);
        run({4}, true, 1);
        run({4}, true, 2);
        run({4}, true, 3);
        run({4}, true, 4);
    }
}

TEST(BulletsFromBunkers, OneBunkerHighGround)
{
    for (int i = 0; i < ITERATIONS_PER_TEST; ++i)
    {
        run({0}, false);
        run({1}, false);
        run({2}, false);
        run({3}, false);
        run({4}, false);
    }
}

TEST(BulletsFromBunkers, OneBunkerWithUnloadingHighGround)
{
    for (int i = 0; i < ITERATIONS_PER_TEST; ++i)
    {
        run({1}, false, 1);
        run({2}, false, 1);
        run({2}, false, 2);
        run({3}, false, 1);
        run({3}, false, 2);
        run({3}, false, 3);
        run({4}, false, 1);
        run({4}, false, 2);
        run({4}, false, 3);
        run({4}, false, 4);
    }
}

TEST(BulletsFromBunkers, OneBunkerLowGroundRange)
{
    for (int i = 0; i < ITERATIONS_PER_TEST; ++i)
    {
        run({0}, true, 0, 0);
        run({1}, true, 0, 0);
        run({2}, true, 0, 0);
        run({3}, true, 0, 0);
        run({4}, true, 0, 0);
    }
}

TEST(BulletsFromBunkers, OneBunkerWithUnloadingLowGroundRange)
{
    for (int i = 0; i < ITERATIONS_PER_TEST; ++i)
    {
        run({1}, true, 1, 0);
        run({2}, true, 1, 0);
        run({2}, true, 2, 0);
        run({3}, true, 1, 0);
        run({3}, true, 2, 0);
        run({3}, true, 3, 0);
        run({4}, true, 1, 0);
        run({4}, true, 2, 0);
        run({4}, true, 3, 0);
        run({4}, true, 4, 0);
    }
}

TEST(BulletsFromBunkers, OneBunkerLowGroundRangeMidway)
{
    for (int i = 0; i < ITERATIONS_PER_TEST; ++i)
    {
        run({0}, true, 0, 330);
        run({1}, true, 0, 330);
        run({2}, true, 0, 330);
        run({3}, true, 0, 330);
        run({4}, true, 0, 330);
    }
}

TEST(BulletsFromBunkers, OneBunkerWithUnloadingLowGroundRangeMidway)
{
    for (int i = 0; i < ITERATIONS_PER_TEST; ++i)
    {
        run({1}, true, 1, 330);
        run({2}, true, 1, 330);
        run({2}, true, 2, 330);
        run({3}, true, 1, 330);
        run({3}, true, 2, 330);
        run({3}, true, 3, 330);
        run({4}, true, 1, 330);
        run({4}, true, 2, 330);
        run({4}, true, 3, 330);
        run({4}, true, 4, 330);
    }
}

TEST(BulletsFromBunkers, TwoBunkersLowGround)
{
    for (int i = 0; i < ITERATIONS_PER_TEST; ++i)
    {
        run({0, 0});
        run({0, 1});
        run({0, 2});
        run({0, 3});
        run({0, 4});
        run({1, 0});
        run({1, 1});
        run({1, 2});
        run({1, 3});
        run({1, 4});
        run({2, 0});
        run({2, 1});
        run({2, 2});
        run({2, 3});
        run({2, 4});
        run({3, 0});
        run({3, 1});
        run({3, 2});
        run({3, 3});
        run({3, 4});
        run({4, 0});
        run({4, 1});
        run({4, 2});
        run({4, 3});
        run({4, 4});
    }
}

TEST(BulletsFromBunkers, TwoBunkersLowGroundWithUnloading)
{
    for (int i = 0; i < ITERATIONS_PER_TEST; ++i)
    {
        run({0, 1}, true, 1);
        run({0, 2}, true, 1);
        run({0, 3}, true, 2);
        run({0, 4}, true, 2);
        run({1, 0}, true, 1);
        run({1, 1}, true, 1);
        run({1, 2}, true, 2);
        run({1, 3}, true, 3);
        run({1, 4}, true, 4);
        run({2, 0}, true, 1);
        run({2, 1}, true, 1);
        run({2, 2}, true, 2);
        run({2, 3}, true, 1);
        run({2, 4}, true, 5);
        run({3, 0}, true, 1);
        run({3, 1}, true, 1);
        run({3, 2}, true, 4);
        run({3, 3}, true, 6);
        run({3, 4}, true, 1);
        run({4, 0}, true, 1);
        run({4, 1}, true, 1);
        run({4, 2}, true, 3);
        run({4, 3}, true, 6);
        run({4, 4}, true, 4);
    }
}
