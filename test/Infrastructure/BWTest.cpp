#include "BWTest.h"

#include "BW/BWData.h"
#include "StardustAIModule.h"
#include <chrono>
#include <thread>
#include <csignal>
#include <execinfo.h>
#include <filesystem>
#include <sys/shm.h>
#include <random>

#include "Geo.h"

const double pi = 3.14159265358979323846;

namespace
{
    std::mt19937 rng((std::random_device()) ());

    template<typename It>
    It randomElement(It start, It end)
    {
        std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
        std::advance(start, dis(rng));
        return start;
    }

    int scheduleInitialUnitCreation(std::vector<UnitTypeAndPosition> &initialUnits,
                                    std::unordered_map<int, std::vector<UnitTypeAndPosition>> &initialUnitsByFrame)
    {
        // Rules for creating units:
        // - First create workers and overlords
        // - Then create pylons
        // - Then create non-combat buildings
        // - Then create combat buildings
        // - Then create remaining units

        bool changed = false;
        int frame = 0;

        // Scan for workers
        for (auto it = initialUnits.begin(); it != initialUnits.end();)
        {
            if (it->type.isWorker() || it->type == BWAPI::UnitTypes::Zerg_Overlord)
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

        // Scan for buildings where we want to wait for creep
        for (auto it = initialUnits.begin(); it != initialUnits.end();)
        {
            if (it->waitForCreep)
            {
                initialUnitsByFrame[frame].push_back(*it);
                it = initialUnits.erase(it);
                frame += 1000;
            }
            else
            {
                it++;
            }
        }

        // Scan for pylons
        changed = false;
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

    void moveFileToReadIfExists(const std::string &filename)
    {
        if (!std::filesystem::exists(filename)) return;

        std::filesystem::create_directories("bwapi-data/read");

        std::filesystem::rename(
                filename,
                (std::ostringstream() << "bwapi-data/read/" << filename.substr(filename.rfind('/') + 1)).str());
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
}

BWAPI::Position UnitTypeAndPosition::getCenterPosition() const
{
    if (tilePosition != BWAPI::TilePositions::Invalid)
    {
        return Geo::CenterOfUnit(tilePosition, type);
    }

    if (isCenter) return position;

    return Geo::CenterOfUnit(position, type);
}

void BWTest::run()
{
    // If in connect mode, just run the game directly
    // In this mode, both players are running in their own process
    if (host || connect)
    {
        runGame(false);
        return;
    }

    // Ensure a map is selected
    if (!map)
    {
        // If no map set is defined on the test, default to all SSCAIT maps
        if (maps.empty())
        {
            maps = Maps::Get("sscai");
        }

        map = std::make_shared<Maps::MapMetadata>(*randomElement(maps.begin(), maps.end()));
    }

    // If the random seed is -1, generate one
    if (randomSeed == -1)
    {
        std::uniform_int_distribution<> distribution(1, 100000);
        randomSeed = distribution(rng);
    }

    initialUnitFrames = std::max(
            scheduleInitialUnitCreation(myInitialUnits, myInitialUnitsByFrame),
            scheduleInitialUnitCreation(opponentInitialUnits, opponentInitialUnitsByFrame));

    auto shmid = shmget(IPC_PRIVATE, 256, IPC_CREAT | 0666);
    if (shmid < 0)
    {
        std::cerr << "Unable to create shared memory: " << errno << std::endl;
    }
    else
    {
        sharedMemory = (char *) shmat(shmid, nullptr, 0);
        memset(sharedMemory, 0, 256);
    }

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

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        runGame(true);
        _exit(EXIT_SUCCESS);
    }

    auto handler = [](int sig)
    {
        signalHandler(sig, false);
    };

    signal(SIGFPE, handler);
    signal(SIGSEGV, handler);
    signal(SIGABRT, handler);

    runGame(false);

    // Give the opponent 5 seconds to exit
    int tries = 0;
    while (true)
    {
        if (waitpid(opponentPid, nullptr, WNOHANG) != -1)
        {
            std::cout << "Opponent process exited" << std::endl;
            break;
        }

        // Kill after 5 seconds
        tries++;
        if (tries == 50)
        {
            kill(opponentPid, SIGKILL);
            std::cout << "Opponent process killed" << std::endl;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (shmid >= 0)
    {
        shmdt(sharedMemory);
        shmctl(shmid, IPC_RMID, nullptr);
    }
}

void BWTest::addClockPositionToReplayName()
{
    // First get the positions of the start location and center of the map
    auto startPos = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()) + BWAPI::Position(64, 48);
    auto centerPos = BWAPI::Position(BWAPI::TilePosition(BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight())) / 2;

    // Calculate the angle
    auto angle = atan2(centerPos.y - startPos.y, centerPos.x - startPos.x);

    // Adjust the frame of reference so 0 is up and normalize so all numbers are positive
    angle -= (pi / 2.0);
    if (angle < 0) angle += (pi * 2.0);

    // Round to nearest clock direction and convert 0 to 12
    auto clockDirection = std::lround(angle / (pi / 6.0));
    if (clockDirection == 0) clockDirection = 12;

    std::ostringstream replayNameBuilder;
    replayNameBuilder << replayName;
    replayNameBuilder << "_" << clockDirection << "_OClock";
    replayName = replayNameBuilder.str();
}

void BWTest::runGame(bool opponent)
{
    BW::GameOwner gameOwner;
    BWAPI::BroodwarImpl_handle h(gameOwner.getGame());
    h->setCharacterName(opponent ? opponentName : "Startest");
    h->setGameType(BWAPI::GameTypes::Melee);
    BWAPI::BroodwarImpl.bwgame.setMapFileName(map->filename);
    BWAPI::Race race = opponent ? opponentRace : myRace;
    h->createMultiPlayerGame([&]()
                             {
                                 if (h->self())
                                 {
                                     if (h->self()->getRace() != race) h->self()->setRace(race);
                                 }
                                 else
                                 {
                                     h->switchToPlayer(h->getPlayer((opponent || connect) ? 1 : 0));
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

    std::cout << "Game started" << (opponent ? " (opponent)" : "") << "! "
              << "framelimit=" << frameLimit
              << "; timelimit=" << timeLimit
              << "; map=" << map->filename
              << "; seed=" << randomSeed
              << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    bool isStardustModule = false;
    auto setTestHooks = [&](BWAPI::AIModule *module)
    {
        auto stardustModule = dynamic_cast<StardustAIModule*>(module);
        if (!stardustModule)
        {
            module->afterOnStart = [&]()
            {
                h->setLocalSpeed(0);

                if (opponent && onStartOpponent) onStartOpponent();
                if (!opponent && onStartMine) onStartMine();
            };
            return;
        }

        if (opponent)
        {
            stardustModule->testOnStart = onStartOpponent;
            stardustModule->testOnFrame = onFrameOpponent;
            stardustModule->testOnEnd = onEndOpponent;
        }
        else
        {
            stardustModule->testOnStart = onStartMine;
            stardustModule->testOnFrame = onFrameMine;
            stardustModule->testOnEnd = onEndMine;
        }
        stardustModule->afterOnStart = [&]()
        {
            h->setLocalSpeed(0);
        };
        isStardustModule = true;
    };

    if (opponent)
    {
        if (opponentModule)
        {
            auto module = opponentModule();
            setTestHooks(module);
            h->setAIModule(module);
        }
    }
    else
    {
        BWAPI::AIModule *module;
        if (myModule)
        {
            module = myModule();
        }
        else
        {
            auto stardustModule = new StardustAIModule();
            if (initialUnitFrames > 0) stardustModule->frameSkip = initialUnitFrames + BWAPI::Broodwar->getLatencyFrames();
            module = stardustModule;
        }
        setTestHooks(module);
        h->setAIModule(module);
        Log::SetOutputToConsole(true);
    }
    h->update();

    if (!opponent && !removeStatic.empty())
    {
        for (auto &tile : removeStatic)
        {
            for (auto &unit : h->getStaticNeutralUnits())
            {
                if (unit->getInitialTilePosition() == tile)
                {
                    h->removeUnit(unit);
                }
            }
        }

        for (int f = 0; f < 4; f++)
        {
            gameOwner.getGame().nextFrame();
            h->update();
        }
    }

    for (int frame = 0; frame <= initialUnitFrames; frame++)
    {
        if (frame > 0) h->update();

        auto &initialUnits = opponent ? opponentInitialUnitsByFrame[frame] : myInitialUnitsByFrame[frame];
        for (auto &unitAndPosition : initialUnits)
        {
            h->createUnit(h->self(), unitAndPosition.type, unitAndPosition.getCenterPosition());
        }

        gameOwner.getGame().nextFrame();
    }

    frameLimit += initialUnitFrames;

    bool leftGame = false;
    auto startTime = std::chrono::high_resolution_clock::now();
    while (!gameOwner.getGame().gameOver())
    {
        try
        {
            h->update();

            if (!isStardustModule && !leftGame)
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
            std::cerr << "Exception caught in frame (" << (opponent ? "opponent" : "mine") << "): " << ex.what() << std::endl;
            printBacktrace();
            if (!leftGame)
            {
                leftGame = true;
                h->leaveGame();
            }
        }
    }

    std::cout << "Game over " << (opponent ? "(opponent) " : "") << "after " << h->getFrameCount() << " frames" << std::endl;

    h->update();

    if (!isStardustModule)
    {
        if (opponent)
        {
            if (onEndOpponent) onEndOpponent(gameOwner.getGame().won());
        }
        else
        {
            if (onEndMine) onEndMine(gameOwner.getGame().won());
        }
    }

    try
    {
        h->onGameEnd();
    }
    catch (std::exception &ex)
    {
        std::cerr << "Exception caught in game end (" << (opponent ? "opponent" : "mine") << "): " << ex.what() << std::endl;
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
                    if (!std::filesystem::exists(logFilename)) continue;

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
    else
    {
        // Move opponent learning files to read
        moveFileToReadIfExists("bwapi-data/write/om_Startest.txt"); // Steamhammer
        moveFileToReadIfExists("bwapi-data/write/omlocutus_startest.txt"); // Locutus
        moveFileToReadIfExists("bwapi-data/write/ZvP Startest AIIDE2021.txt"); // McRave
        moveFileToReadIfExists("bwapi-data/write/ZvP Startest AIIDE2021 Info.txt"); // McRave
    }
    h->bwgame.leaveGame();
}