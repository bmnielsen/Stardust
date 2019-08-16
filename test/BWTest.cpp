#include "BWTest.h"

#include "BW/BWData.h"
#include "LocutusAIModule.h"
#include <chrono>
#include <thread>
#include <signal.h>
#include <execinfo.h>
#include <filesystem>

void signalHandler(int sig)
{
    std::cout << "Crashed with signal " << sig << std::endl;

    void *array[20];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 20);

    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

void BWTest::run()
{
    auto pid = fork();
    if (pid == 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        runGame(true);
        _exit(EXIT_SUCCESS);
    }

    signal(SIGFPE, signalHandler);
    signal(SIGSEGV, signalHandler);

    runGame(false);
    waitpid(pid, nullptr, 0);
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
        if (opponentModule) h->setAIModule(opponentModule);
        if (onStartOpponent) onStartOpponent();
    }
    else
    {
        h->setAIModule(new LocutusAIModule());
        Log::SetOutputToConsole(true);
        if (onStartMine) onStartMine();
    }
    h->update();
    h->setLocalSpeed(0);
    gameOwner.getGame().nextFrame();

    bool leftGame = false;
    auto startTime = std::chrono::high_resolution_clock::now();
    while (!gameOwner.getGame().gameOver())
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
    h->onGameEnd();
    if (opponent)
    {
        if (onEndOpponent) onEndOpponent();
    }
    else
    {
        if (onEndMine) onEndMine();

        auto result = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count();
        std::cout << "Total game time: " << result << "s" << std::endl;

        EXPECT_TRUE(gameOwner.getGame().won());

        // Create an ID for this game based on the test case and timestamp
        std::ostringstream gameId;
        gameId << ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
        gameId << "_" << ::testing::UnitTest::GetInstance()->current_test_info()->name();
        auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
#pragma warning(disable : 4996)
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

        // Write the replay file
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

            for (auto logFilename : Log::LogFiles())
            {
                std::ostringstream newLogFilename;
                newLogFilename << logDirectory.str() << "/" << logFilename.substr(logFilename.rfind("/") + 1);
                std::filesystem::rename(logFilename, newLogFilename.str());
            }
        }
    }
    h->bwgame.leaveGame();
}