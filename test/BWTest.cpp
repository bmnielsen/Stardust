#include "BWTest.h"

#include "BW/BWData.h"
#include "LocutusAIModule.h"
#include <chrono>
#include <thread>
#include <signal.h>
#include <execinfo.h>
#include <filesystem>
#include <sys/shm.h>

#include "Geo.h"

namespace
{
    int scheduleInitialUnitCreation(std::vector<UnitTypeAndPosition> &initialUnits,
                                    std::unordered_map<int, std::vector<UnitTypeAndPosition>> &initialUnitsByFrame)
    {
        // Rules for creating units:
        // - Create buildings before non-buildings
        // - Create non-combat buildings before combat buildings
        // - Create pylons before buildings requiring power

        bool changed = false;
        int frame = 0;

        // Scan for pylons
        for (auto it = initialUnits.begin(); it != initialUnits.end();)
        {
            if (it->type == BWAPI::UnitTypes::Protoss_Pylon)
            {
                initialUnitsByFrame[frame].push_back(*it);
                it = initialUnits.erase(it);
                changed = true;
            }
            else
            {
                it++;
            }
        }

        if (changed) frame++;

        // Scan for non-combat buildings
        changed = false;
        for (auto it = initialUnits.begin(); it != initialUnits.end();)
        {
            if (it->type.isBuilding() && !it->type.canAttack())
            {
                initialUnitsByFrame[frame].push_back(*it);
                it = initialUnits.erase(it);
                changed = true;
            }
            else
            {
                it++;
            }
        }

        if (changed) frame++;

        // Scan for combat buildings
        changed = false;
        for (auto it = initialUnits.begin(); it != initialUnits.end();)
        {
            if (it->type.isBuilding())
            {
                initialUnitsByFrame[frame].push_back(*it);
                it = initialUnits.erase(it);
                changed = true;
            }
            else
            {
                it++;
            }
        }

        if (changed) frame++;

        // Add remaining units
        for (auto &initialUnit : initialUnits)
        {
            initialUnitsByFrame[frame].push_back(initialUnit);
        }

        return frame;
    }

    void printBacktrace()
    {
        void *array[20];
        size_t size;

        // get void*'s for all entries on the stack
        size = backtrace(array, 20);

        // print out all the frames to stderr
        backtrace_symbols_fd(array, size, STDERR_FILENO);
    }
}

void signalHandler(int sig, bool opponent)
{
    if (opponent)
    {
        std::cerr << "Opponent crashed with signal " << sig << std::endl;
    }
    else
    {
        EXPECT_FALSE(true);
        std::cerr << "Crashed with signal " << sig << std::endl;
    }

    fprintf(stderr, "Error: signal %d:\n", sig);
    printBacktrace();
    exit(1);
}

BWAPI::Position UnitTypeAndPosition::getCenterPosition()
{
    if (tilePosition.isValid())
    {
        return Geo::CenterOfUnit(tilePosition, type);
    }

    return Geo::CenterOfUnit(position, type);
}

void BWTest::run()
{
    initialUnitFrames = std::max(
            scheduleInitialUnitCreation(myInitialUnits, myInitialUnitsByFrame),
            scheduleInitialUnitCreation(opponentInitialUnits, opponentInitialUnitsByFrame));

    auto shmid = shmget(IPC_PRIVATE, 256, IPC_CREAT | 0666);
    sharedMemory = (char *) shmat(shmid, nullptr, 0);
    memset(sharedMemory, 0, 256);

    auto opponentPid = fork();
    if (opponentPid == 0)
    {
        auto handler = [](int sig)
        {
            signalHandler(sig, true);
        };
        signal(SIGFPE, handler);
        signal(SIGSEGV, handler);
        signal(SIGABRT, handler);

        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        runGame(true);
        _exit(EXIT_SUCCESS);
    }

    // Unless we are debugging, we run our bot in a separate process to ensure we
    // get all of our globals reset if multiple tests are being run
#ifdef DEBUG
    auto handler = [](int sig)
    {
        signalHandler(sig, false);
    };

    signal(SIGFPE, handler);
    signal(SIGSEGV, handler);
    signal(SIGABRT, handler);

    runGame(false);
#else
    auto selfPid = fork();
    if (selfPid == 0)
    {
        auto handler = [](int sig)
        {
            signalHandler(sig, false);
        };
        signal(SIGFPE, handler);
        signal(SIGSEGV, handler);
        signal(SIGABRT, handler);

        runGame(false);

        _exit(::testing::Test::HasFailure() ? EXIT_FAILURE : EXIT_SUCCESS);
    }

    int result;
    waitpid(selfPid, &result, 0);
    EXPECT_EQ(result, 0);
#endif

    waitpid(opponentPid, nullptr, 0);

    shmdt(sharedMemory);
    shmctl(shmid, IPC_RMID, nullptr);
}

void BWTest::runGame(bool opponent)
{
    BW::GameOwner gameOwner;
    BWAPI::BroodwarImpl_handle h(gameOwner.getGame());
    h->setCharacterName(opponent ? "Opponent" : "Testcutus");
    h->setGameType(BWAPI::GameTypes::Melee);
    BWAPI::BroodwarImpl.bwgame.setMapFileName(map);
    BWAPI::Race race = opponent ? opponentRace : BWAPI::Races::Protoss;
    h->createMultiPlayerGame([&]()
                             {
                                 if (h->self())
                                 {
                                     if (h->self()->getRace() != race) h->self()->setRace(race);
                                 }
                                 else
                                 {
                                     h->switchToPlayer(h->getPlayer(opponent ? 1 : 0));
                                 }

                                 int playerCount = 0;
                                 for (int i = 0; i < BW::PLAYABLE_PLAYER_COUNT; ++i)
                                 {
                                     BWAPI::Player p = h->getPlayer(i);
                                     if (p->getType() != BWAPI::PlayerTypes::Player
                                         && p->getType() != BWAPI::PlayerTypes::Computer)
                                         continue;
                                     ++playerCount;
                                 }
                                 if (playerCount >= 2)
                                 {
                                     h->setRandomSeed(randomSeed);
                                     h->startGame();
                                 }
                             });

    std::cout << "Game started!" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    if (opponent)
    {
        if (opponentModule)
        {
            auto module = opponentModule();
            module->afterOnStart = [this, &h]()
            {
                h->setLocalSpeed(0);

                if (onStartOpponent) onStartOpponent();
            };
            h->setAIModule(module);
        }
    }
    else
    {
        auto module = myModule ? myModule() : new LocutusAIModule();
        module->afterOnStart = [this, &h]()
        {
            h->setLocalSpeed(0);

            if (onStartMine) onStartMine();
        };
        h->setAIModule(module);
        Log::SetOutputToConsole(true);
    }
    h->update();

    for (int frame = 0; frame <= initialUnitFrames; frame++)
    {
        if (frame > 0) h->update();

        if (opponent)
        {
            for (auto &unitAndPosition : opponentInitialUnitsByFrame[frame])
            {
                h->createUnit(h->getPlayer(1), unitAndPosition.type, unitAndPosition.getCenterPosition());
            }

            if (onFrameOpponent) onFrameOpponent();
        }
        else
        {
            for (auto &unitAndPosition : myInitialUnitsByFrame[frame])
            {
                h->createUnit(h->getPlayer(0), unitAndPosition.type, unitAndPosition.getCenterPosition());
            }

            if (onFrameMine) onFrameMine();
        }

        gameOwner.getGame().nextFrame();
    }

    bool leftGame = false;
    auto startTime = std::chrono::high_resolution_clock::now();
    while (!gameOwner.getGame().gameOver())
    {
        try
        {
            h->update();

            if (!leftGame)
            {
                if (opponent)
                {
                    if (onFrameOpponent) onFrameOpponent();
                }
                else
                {
                    if (onFrameMine) onFrameMine();
                }
            }

            if (!leftGame && h->getFrameCount() == frameLimit)
            {
                std::cout << "Frame limit reached; leaving game" << std::endl;
                leftGame = true;
                h->leaveGame();
            }

            if (!leftGame)
            {
                auto now = std::chrono::high_resolution_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count() > timeLimit)
                {
                    std::cout << "Time limit reached; leaving game" << std::endl;
                    leftGame = true;
                    h->leaveGame();
                }
            }

            gameOwner.getGame().nextFrame();
        }
        catch (std::exception &ex)
        {
            std::cout << "Exception caught in frame (" << (opponent ? "opponent" : "mine") << "): " << ex.what() << std::endl;
            printBacktrace();
            if (!leftGame)
            {
                leftGame = true;
                h->leaveGame();
            }
        }
    }

    h->update();

    if (opponent)
    {
        if (onEndOpponent) onEndOpponent();
    }
    else
    {
        if (onEndMine) onEndMine();
    }

    try
    {
        h->onGameEnd();
    }
    catch (std::exception &ex)
    {
        std::cout << "Exception caught in game end (" << (opponent ? "opponent" : "mine") << "): " << ex.what() << std::endl;
        printBacktrace();
    }

    if (!opponent)
    {
        auto result = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count();
        std::cout << "Total game time: " << result << "s" << std::endl;

        if (expectWin) EXPECT_TRUE(gameOwner.getGame().won());

        // Create an ID for this game based on the test case and timestamp
        std::ostringstream gameId;
        if (replayName.empty())
        {
            gameId << ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
            gameId << "_" << ::testing::UnitTest::GetInstance()->current_test_info()->name();
        }
        else
        {
            gameId << replayName;
        }
        auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        auto tm = std::localtime(&tt);
        gameId << "_" << std::put_time(tm, "%Y%m%d_%H%M%S");
        if (::testing::UnitTest::GetInstance()->current_test_info()->result()->Failed())
        {
            gameId << "_FAIL";
        }
        else
        {
            gameId << "_PASS";
        }

        // If enabled, write the replay file
        // Otherwise remove the cvis directory
        if (writeReplay)
        {
            std::ostringstream replayFilename;
            replayFilename << "replays/" << gameId.str() << ".rep";
            std::filesystem::create_directories("replays");
            BWAPI::BroodwarImpl.bwgame.saveReplay(replayFilename.str());

            // Move the cvis directory
            if (std::filesystem::exists("bwapi-data/write/cvis"))
            {
                std::ostringstream cvisFilename;
                cvisFilename << "replays/" << gameId.str() << ".rep.cvis";
                std::filesystem::rename("bwapi-data/write/cvis", cvisFilename.str());
            }

            // Move log files
            if (!Log::LogFiles().empty())
            {
                std::ostringstream logDirectory;
                logDirectory << "replays/" << gameId.str() << ".rep.log";
                std::filesystem::create_directories(logDirectory.str());

                for (auto &logFilename : Log::LogFiles())
                {
                    std::ostringstream newLogFilename;
                    newLogFilename << logDirectory.str() << "/" << logFilename.substr(logFilename.rfind('/') + 1);
                    std::filesystem::rename(logFilename, newLogFilename.str());
                }
            }
        }
        else
        {
            if (std::filesystem::exists("bwapi-data/write/cvis"))
            {
                std::filesystem::remove_all("bwapi-data/write/cvis");
            }
        }
    }
    h->bwgame.leaveGame();
}