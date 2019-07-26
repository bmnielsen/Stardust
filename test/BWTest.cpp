#include "BWTest.h"

#include "BW/BWData.h"
#include "LocutusAIModule.h"
#include <chrono>
#include <thread>

void BWTest::run()
{
    auto pid = fork();
    if (pid == 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        runGame(true);
        _exit(EXIT_SUCCESS);
    }

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
                                     h->setRandomSeed(5);
                                     h->startGame();
                                 }
                             });

    std::cout << "Game started!" << std::endl;

    if (opponent)
    {
        if (opponentModule) h->setAIModule(opponentModule);
        if (onStartOpponent) onStartOpponent(h);
    }
    else
    {
        h->setAIModule(new LocutusAIModule());
        Log::SetOutputToConsole(true);
        if (onStartMine) onStartMine(h);
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
                if (onFrameOpponent) onFrameOpponent(h);
            }
            else
            {
                if (onFrameMine) onFrameMine(h);
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
    if (opponent)
    {
        if (onEndOpponent) onEndOpponent(h);
    }
    else
    {
        if (onEndMine) onEndMine(h);

        std::ostringstream filename;
        filename << "replays/";
        filename << ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
        filename << "_" << ::testing::UnitTest::GetInstance()->current_test_info()->name();
        auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
#pragma warning(disable : 4996)
        auto tm = std::localtime(&tt);
        filename << "_" << std::put_time(tm, "%Y%m%d_%H%M%S");
        filename << ".rep";
        BWAPI::BroodwarImpl.bwgame.saveReplay(filename.str());
    }
    h->onGameEnd();
    h->bwgame.leaveGame();
}