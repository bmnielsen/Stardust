#include "ShushanKingAIModule.h"

#include <chrono>
/* SEH crash guard (B7) needs EXCEPTION_POINTERS et al from windows.h. */
/* WIN32_LEAN_AND_MEAN excludes richedit.h whose `small` macro breaks Block.h; */
/* NOMINMAX keeps std::min/max usable. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "Timer.h"
#include "Map.h"
#include "NoGoAreas.h"
#include "PathFinding.h"
#include "Producer.h"
#include "Builder.h"
#include "BuildingPlacement.h"
#include "Opponent.h"
#include "Strategist.h"
#include "General.h"
#include "Units.h"
#include "Workers.h"
#include "MiningOptimization/WorkerMiningOptimization.h"
#include "WorkerMiningInstrumentation.h"
#include "Bullets.h"
#include "Players.h"
#include "Geo.h"
#include "Config.h"
#include "FileTools.h"
#include <fstream>

int frameNow;

/* While instrumenting we have a lower global frame limit to ensure we get data if the game locks up */
#if INSTRUMENTATION_ENABLED_VERBOSE
#define FRAME_LIMIT 25000
#else
#if INSTRUMENTATION_ENABLED
#define FRAME_LIMIT 40000
#endif
#endif

#if INSTRUMENTATION_ENABLED_VERBOSE

/* Heatmaps are quite large, so we don't always want to write them every frame */
/* These defines configure what frequency to dump them, or 0 to disable them */
#define COLLISION_HEATMAP_FREQUENCY_ENEMY 0
#define GROUND_THREAT_HEATMAP_FREQUENCY_ENEMY 0
#define GROUND_THREAT_STATIC_HEATMAP_FREQUENCY_ENEMY 0
#define AIR_THREAT_HEATMAP_FREQUENCY_ENEMY 0
#define DETECTION_HEATMAP_FREQUENCY_ENEMY 0
#define STASIS_RANGE_HEATMAP_FREQUENCY_ENEMY 0

#define COLLISION_HEATMAP_FREQUENCY_MINE 0
#define GROUND_THREAT_HEATMAP_FREQUENCY_MINE 0
#define GROUND_THREAT_STATIC_HEATMAP_FREQUENCY_MINE 0
#define AIR_THREAT_HEATMAP_FREQUENCY_MINE 0
#define DETECTION_HEATMAP_FREQUENCY_MINE 0

#define POWER_HEATMAP_FREQUENCY 0 // Only tracked for self
#endif

namespace
{
    bool gameFinished;

    void handleUnitDiscover(BWAPI::Unit gameUnit)
    {
        Map::onUnitDiscover(gameUnit);
    }

    void handleUnitDestroy(BWAPI::Unit gameUnit)
    {
        Map::onUnitDestroy(gameUnit);
        UnitMgr::onUnitDestroy(gameUnit);
    }

    /* L0-lite diagnostics channel: Log is compiled out in release builds */
    /* (LOGGING_ENABLED undefined), so instrumentation writes its own append-only */
    /* file via the standard write-dir contract. */
    void appendStatLine(const std::string &line)
    {
        std::ofstream out(FileTools::getFilePath("shushanking_framestats", "txt", true),
                          std::ofstream::app);
        if (out.is_open()) out << line << "\n";
    }
}

void ShushanKingAIModule::onStart()
{
    gameFinished = false;
    frameNow = 0;

    /* Initialize globals that just need to make sure their global data is reset */
    AppLog::initialize();
    Constructor::initialize();
    Opponent::initialize();
    Marshal::initialize();
    WorkerMgr::initialize();
    Bullets::initialize();
    PlayerInfo::initialize();
    Geom::initialize();
    Pathfinder::clearGrids();
    Pathfinder::initializeSearch();

    AppLog::SetDebug(true);
    Telemetry::initialize();

    if (moduleBaseAddress > 0)
    {
        AppLog::Get() << "Module base address: 0x" << std::hex << moduleBaseAddress;
    }

    Timer::start("Startup");

    /* L0 config (B6): load params.json + strategy library; any failure falls back */
    /* to built-in defaults and is reported on the diagnostics channel. */
    Config::initialize();
    appendStatLine("=== game start === " + Config::statusLine());

    /* HT-pipe diagnostics (2026-09-28): batches 14-16 trained zero HTs. Logs the */
    /* runtime-true config values so a dead key or misnamed parameter (the Amulet */
    /* gate bug) is visible in the archive without source inspection. */
    {
        std::ostringstream htpipe;
        htpipe << "HTPIPE CONFIG enemy=" << Opponent::getName()
               << " enemyRace=" << (BWAPI::Broodwar->enemy() ? BWAPI::Broodwar->enemy()->getRace().toString() : "?")
               << " lateEnabled=" << Config::pvpLateTransitionEnabled()
               << " tf=" << Config::pvpLateTransitionFrame()
               << " htTarget=" << Config::pvpLateTransitionHTTarget()
                << " reaverTarget=" << Config::pvpLateTransitionReaverTarget()
                << " amulet=" << Config::stormAmuletEnabled()
                << " archon=" << Config::archonMergeEnabled()
                << " stormCast=" << Config::stormCastEnabled()
                << " armyComp=" << Config::armyCompositionEnabled()
                << " stormTimingKey=" << Config::stormTimingEnabled();
        FileTools::appendLine("shushanking_htpipe", htpipe.str());
    }

    UnitMgr::initialize();
    Timer::checkpoint("Units::initialize");

    Map::initialize();
    Timer::checkpoint("Map::initialize");

    Pathfinder::initializeGrids();
    Timer::checkpoint("PathFinding::initialize");

    BuildingPlacement::initialize();
    Timer::checkpoint("BuildingPlacement::initialize");

    CombatSim::initialize();
    Timer::checkpoint("CombatSim::initialize");

    WorkerMiningOptimization::initialize();
    Timer::checkpoint("WorkerMiningOptimization::initialize");

    WorkerMiningInstrumentation::initialize();
    Timer::checkpoint("WorkerMiningInstrumentation::initialize");

    Director::initialize();
    Timer::checkpoint("Strategist::initialize");

    Timer::stop(true);

    AppLog::Get() << "Initialized game against " << Opponent::getName()
               << " on " << BWAPI::Broodwar->mapFileName() << " (" << BWAPI::Broodwar->mapHash() << ")";
    AppLog::Get() << "My starting position: " << Map::getMyMain()->getTilePosition();
    if (Map::getMyNatural())
    {
        AppLog::Get() << "My natural position: " << Map::getMyNatural()->getTilePosition();
    }
    else
    {
        AppLog::Get() << "No natural position available";
    }
    if (Map::getMyMainChoke())
    {
        AppLog::Get() << "My main choke: " << BWAPI::TilePosition(Map::getMyMainChoke()->midpoint);
    }
    else
    {
        AppLog::Get() << "No main choke available";
    }

    if (testOnStart) testOnStart();
}

void ShushanKingAIModule::onEnd(bool isWinner)
{
    if (testOnEnd) testOnEnd(isWinner);

#if LOGGING_ENABLED
    WorkerMiningInstrumentation::writeGameEndInstrumentation();
#endif

    Opponent::gameEnd(isWinner);
    WorkerMiningOptimization::write();
    Telemetry::gameEnd();
}

/* --- L0-lite (D4-5): B7 crash guard + A4 measure-only frame governor --- */
namespace
{
    bool frameLogicDegraded = false;   // B7: set after an SEH exception; we then no-op
    int crashGuardHits = 0;

        /* A4 measure-only stats (no intervention yet; ladder comes in Phase 4) */
        long long frameMicrosAccum = 0;
        long long frameMicrosMax = 0;
        int framesOver35ms = 0;
        int framesSampled = 0;

        /* A4 ladder (S5): yield a frame after an over-limit frame, at most */
        /* skipMax consecutive yields (then run anyway - starving the logic */
        /* entirely loses games just as surely as a disqualification would). */
        bool ladderPrevOver = false;
        int ladderConsecSkips = 0;
        long long ladderTotalSkips = 0;

    void logFrameStats(int frameNo)
    {
        if (framesSampled == 0) return;
        double avgMs = (double) frameMicrosAccum / framesSampled / 1000.0;
        double maxMs = (double) frameMicrosMax / 1000.0;
        std::ostringstream os;
        os << "FrameStats @" << frameNo << ": avg=" << avgMs << "ms max=" << maxMs
           << "ms over35ms=" << framesOver35ms << "/" << framesSampled
           << " ladderSkips=" << ladderTotalSkips
           << (frameLogicDegraded ? " [DEGRADED]" : "");
        AppLog::Get() << os.str();
        appendStatLine(os.str());
        frameMicrosAccum = 0; frameMicrosMax = 0; framesOver35ms = 0; framesSampled = 0;
    }

    int frameGuardFilter(unsigned int code, EXCEPTION_POINTERS *)
    {
        crashGuardHits++;
        std::ostringstream os;
        os << "CRASH GUARD: SEH exception 0x" << std::hex << code
           << " at frame " << std::dec << BWAPI::Broodwar->getFrameCount()
           << " (hit #" << crashGuardHits << ") - degrading to minimal mode";
        AppLog::Get() << os.str();
        appendStatLine(os.str());
        return EXCEPTION_EXECUTE_HANDLER;
    }
}

void ShushanKingAIModule::onFrame()
{
    /* B7: keep this wrapper free of objects with destructors: SEH (__try) */
    /* cannot coexist with C++ unwinding (C2712). Degraded mode = no-op frames; */
    /* units keep their last orders (workers keep mining) and onEnd still writes */
    /* the learning record. */
    if (frameLogicDegraded) return;

    /* A4 ladder (S5): after an over-limit frame, yield this frame's heavy */
    /* decision layers (pump above the split always runs; see onFrameGuarded) */
    /* up to skipMax consecutive times. Inactive unless params */
    /* frame_governor.mode == "ladder". */
    bool runHeavy = true;
    if (ladderPrevOver && Config::governorLadder()
        && ladderConsecSkips < Config::governorSkipMax())
    {
        runHeavy = false;
        ladderConsecSkips++;
        ladderTotalSkips++;
    }

    long long t0 = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

    __try
    {
        onFrameGuarded(runHeavy);
    }
    __except (frameGuardFilter(GetExceptionCode(), GetExceptionInformation()))
    {
        frameLogicDegraded = true;
    }

    long long dt = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() - t0;
    frameMicrosAccum += dt;
    if (dt > frameMicrosMax) frameMicrosMax = dt;
    if (dt > 35000) framesOver35ms++;
    framesSampled++;

    /* A4 ladder bookkeeping (see the yield branch above) */
    if (Config::governorLadder())
    {
        ladderPrevOver = dt > (long long) Config::governorLimitMs() * 1000;
        if (!ladderPrevOver) ladderConsecSkips = 0;
    }

    if (framesSampled >= 1000) logFrameStats(BWAPI::Broodwar->getFrameCount());
}

void ShushanKingAIModule::onFrameGuarded(bool runHeavy)
{
    if (frameNow < frameSkip)
    {
        /* We skip one frame at the start so our frame aligns with the game data, see readme in mining optimization for rationale */
        if (BWAPI::Broodwar->getFrameCount() > 0) frameNow++;
        return;
    }
    if (gameFinished) return;

    if (BWAPI::Broodwar->isPaused()) return;
    if (BWAPI::Broodwar->isReplay()) return;

#ifdef FRAME_LIMIT
    if (enableFrameLimit && frameNow > FRAME_LIMIT)
    {
        AppLog::Get() << "Frame limit reached; leaving game";
        Opponent::setGameValue("ourLeaveGame", frameNow); // multi-signal record (v1.6 L5)
        gameFinished = true;
        BWAPI::Broodwar->leaveGame();
        return;
    }
#endif

    Timer::start("Frame");

    /* Before doing anything else, check if the opponent has left or been eliminated */
    for (auto &event : BWAPI::Broodwar->getEvents())
    {
        if (event.getType() == BWAPI::EventType::PlayerLeft && event.getPlayer() == BWAPI::Broodwar->enemy()
            && frameNow > 100)
        {
            AppLog::Get() << "Opponent has left the game";
            gameFinished = true;
            return;
        }
    }

    /* First priority is to update unit-related things, as most of our other stuff relies on our unit abstraction being updated */

    /* We start with bullets though as they interact directly with units */
    Bullets::update();
    Timer::checkpoint("Bullets::update");

    UnitMgr::update();
    Timer::checkpoint("Units::update");

    /* We handle events explicitly instead of through the event handlers so we can time them */
    for (auto &event : BWAPI::Broodwar->getEvents())
    {
        switch (event.getType())
        {
            case BWAPI::EventType::UnitDiscover:
                handleUnitDiscover(event.getUnit());
                break;
            case BWAPI::EventType::UnitDestroy:
                handleUnitDestroy(event.getUnit());
                break;
            case BWAPI::EventType::ReceiveText:
                AppLog::Get() << "Received text: " << event.getText();
                break;
            default:
                break;
        }
    }
    Timer::checkpoint("Events");

    /* A4 ladder (S5): pump split. Everything above (bullets, unit bookkeeping, */
    /* the event pump) must run EVERY frame - skipping it loses unit discover/ */
    /* destroy events and leaves dangling unit handles, which is exactly the */
    /* 0xc0000005 the forced-trigger smoke caught. What a yielded frame skips */
    /* is the heavy decision stack below. */
    if (!runHeavy) return;

    /* Update general information things */
    Opponent::update();
    Timer::checkpoint("Opponent::update");

    PlayerInfo::update();
    Timer::checkpoint("Players::update");

    WorkerMiningInstrumentation::update();
    Timer::checkpoint("WorkerMiningInstrumentation::update");

    Map::update();
    Timer::checkpoint("Map::update");

    Marshal::updateClusters();
    Timer::checkpoint("General::updateClusters");

    BuildingPlacement::update();
    Timer::checkpoint("BuildingPlacement::update");

    Constructor::update();
    Timer::checkpoint("Builder::update");

    WorkerMgr::updateAssignments();
    Timer::checkpoint("Workers::updateAssignments");

    /* Strategist is what makes all of the big decisions */
    Director::update();
    Timer::checkpoint("Strategist::update");

    /* Hook our test infrastructure in here in case we want to have our units do different stuff */
    if (testOnFrame) testOnFrame();

    /* Update stuff that issues orders */
    Marshal::issueOrders();
    Timer::checkpoint("General::issueOrders");

    Production::update();
    Timer::checkpoint("Producer::update");

    Constructor::issueOrders();
    Timer::checkpoint("Builder::issueOrders");

    /* Called after the above to allow workers to be reassigned for combat, scouting, and building first */
    WorkerMgr::issueOrders();
    Timer::checkpoint("Workers::issueOrders");

    /* Must be last, as this is what executes move orders queued earlier */
    UnitMgr::issueOrders();
    Timer::checkpoint("Units::issueOrders");

    /* Updates the mining optimization data */
    WorkerMiningOptimization::flushObservations();
    Timer::checkpoint("WorkerMiningOptimization::flushObservations");

#if LOGGING_ENABLED
    auto enemyNatural = Map::getEnemyStartingNatural();
    if (enemyNatural && enemyNatural->resourceDepot && enemyNatural->resourceDepot->rawUnit->isVisible()
        && enemyNatural->resourceDepot->health > 100)
    {
        /* Check for other visible enemy units */
        std::set<Unit> enemyUnits;
        UnitMgr::enemyInRadius(enemyUnits, enemyNatural->getPosition(), 640, [](const Unit &gameUnit){ return !gameUnit->type.isBuilding() && gameUnit->rawUnit->isVisible();});
        if (enemyUnits.empty())
        {
            int dragoons = 0;
            for (auto &gameUnit : UnitMgr::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Dragoon))
            {
                if (gameUnit->cooldownUntil > frameNow) continue;
                if (gameUnit->isInOurWeaponRange(enemyNatural->resourceDepot)) dragoons++;
            }

            if (dragoons > 3)
            {
                AppLog::Get() << "ERROR: Multiple dragoons aren't attacking lone enemy natural!";
            }
        }
    }
#endif

    /* Surrender logic */
    if (enableSurrender)
    {
        /* Surrender if the following is true: */
        /* - We have no workers left and no money / depot to create a new one */
        /* - We have no mobile combat units */
        /* - There is at least one enemy combat unit in our main base */
        if (UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Probe) == 0 &&
            (UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Nexus) == 0 || BWAPI::Broodwar->self()->minerals() < 50))
        {
            bool weHaveAMobileCombatUnit = false;
            for (auto &gameUnit : UnitMgr::allMine())
            {
                if (!gameUnit->canAttackGround()) continue;
                if (!gameUnit->type.canMove()) continue;
                weHaveAMobileCombatUnit = true;
                break;
            }

            if (!weHaveAMobileCombatUnit)
            {
                bool enemyInBase = false;
                for (auto &baseLoc : Map::getMyBases())
                {
                    if (enemyInBase) break;
                    for (auto &gameUnit : UnitMgr::enemyAtBase(baseLoc))
                    {
                        if (!gameUnit->canAttackGround()) continue;
                        if (gameUnit->type.isWorker()) continue;
                        enemyInBase = true;
                        break;
                    }
                }

                if (enemyInBase)
                {
                    AppLog::Get() << "Surrendering";
                    BWAPI::Broodwar->sendText("gg");
                    Opponent::setGameValue("ourLeaveGame", frameNow); // multi-signal record (v1.6 L5)
                    gameFinished = true;
                    BWAPI::Broodwar->leaveGame();
                }
            }
        }
    }

    /* Instrumentation */
    NoGoAreas::writeInstrumentation();
    Marshal::writeInstrumentation();
    WorkerMiningInstrumentation::writeInstrumentation();

#if COLLISION_HEATMAP_FREQUENCY_ENEMY
    if (frameNow % COLLISION_HEATMAP_FREQUENCY_ENEMY == 0)
    {
        PlayerInfo::grid(BWAPI::Broodwar->enemy()).dumpCollisionHeatmapIfChanged("CollisionEnemy");
    }
#endif
#if COLLISION_HEATMAP_FREQUENCY_MINE
    if (frameNow % COLLISION_HEATMAP_FREQUENCY_MINE == 0)
    {
        PlayerInfo::grid(BWAPI::Broodwar->self()).dumpCollisionHeatmapIfChanged("CollisionMine");
    }
#endif
#if GROUND_THREAT_HEATMAP_FREQUENCY_ENEMY
    if (frameNow % GROUND_THREAT_HEATMAP_FREQUENCY_ENEMY == 0)
    {
        PlayerInfo::grid(BWAPI::Broodwar->enemy()).dumpGroundThreatHeatmapIfChanged("GroundThreatEnemy");
    }
#endif
#if GROUND_THREAT_HEATMAP_FREQUENCY_MINE
    if (frameNow % GROUND_THREAT_HEATMAP_FREQUENCY_MINE == 0)
    {
        PlayerInfo::grid(BWAPI::Broodwar->self()).dumpGroundThreatHeatmapIfChanged("GroundThreatMine");
    }
#endif
#if GROUND_THREAT_STATIC_HEATMAP_FREQUENCY_ENEMY
    if (frameNow % GROUND_THREAT_STATIC_HEATMAP_FREQUENCY_ENEMY == 0)
    {
        PlayerInfo::grid(BWAPI::Broodwar->enemy()).dumpStaticGroundThreatHeatmapIfChanged("GroundThreatStaticEnemy");
    }
#endif
#if GROUND_THREAT_STATIC_HEATMAP_FREQUENCY_MINE
    if (frameNow % GROUND_THREAT_STATIC_HEATMAP_FREQUENCY_MINE == 0)
    {
        PlayerInfo::grid(BWAPI::Broodwar->self()).dumpStaticGroundThreatHeatmapIfChanged("GroundThreatStaticMine");
    }
#endif
#if AIR_THREAT_HEATMAP_FREQUENCY_ENEMY
    if (frameNow % AIR_THREAT_HEATMAP_FREQUENCY_ENEMY == 0)
    {
        PlayerInfo::grid(BWAPI::Broodwar->enemy()).dumpAirThreatHeatmapIfChanged("AirThreatEnemy");
    }
#endif
#if AIR_THREAT_HEATMAP_FREQUENCY_MINE
    if (frameNow % AIR_THREAT_HEATMAP_FREQUENCY_MINE == 0)
    {
        PlayerInfo::grid(BWAPI::Broodwar->self()).dumpAirThreatHeatmapIfChanged("AirThreatMine");
    }
#endif
#if DETECTION_HEATMAP_FREQUENCY_ENEMY
    if (frameNow % DETECTION_HEATMAP_FREQUENCY_ENEMY == 0)
    {
        PlayerInfo::grid(BWAPI::Broodwar->enemy()).dumpDetectionHeatmapIfChanged("DetectionEnemy");
    }
#endif
#if DETECTION_HEATMAP_FREQUENCY_MINE
    if (frameNow % DETECTION_HEATMAP_FREQUENCY_MINE == 0)
    {
        PlayerInfo::grid(BWAPI::Broodwar->self()).dumpDetectionHeatmapIfChanged("DetectionMine");
    }
#endif
#if STASIS_RANGE_HEATMAP_FREQUENCY_ENEMY
    if (frameNow % STASIS_RANGE_HEATMAP_FREQUENCY_ENEMY == 0)
    {
        PlayerInfo::grid(BWAPI::Broodwar->enemy()).dumpStasisRangeHeatmapIfChanged("StasisRange");
    }
#endif
#if POWER_HEATMAP_FREQUENCY
    if (frameNow % POWER_HEATMAP_FREQUENCY == 0)
    {
        Map::dumpPowerHeatmap();
    }
#endif
    Telemetry::frameEnd();
    Timer::checkpoint("Instrumentation");

    Timer::stop();

    /* We skip one frame at the start so our frame aligns with the game data, see readme in mining optimization for rationale */
    if (BWAPI::Broodwar->getFrameCount() > 0) frameNow++;
}

void ShushanKingAIModule::onSendText(std::string)
{
}

void ShushanKingAIModule::onReceiveText(BWAPI::Player, std::string)
{
}

void ShushanKingAIModule::onPlayerLeft(BWAPI::Player)
{
}

void ShushanKingAIModule::onNukeDetect(BWAPI::Position)
{
}

void ShushanKingAIModule::onUnitDiscover(BWAPI::Unit)
{
}

void ShushanKingAIModule::onUnitEvade(BWAPI::Unit)
{
}

void ShushanKingAIModule::onUnitShow(BWAPI::Unit)
{
}

void ShushanKingAIModule::onUnitHide(BWAPI::Unit)
{
}

void ShushanKingAIModule::onUnitCreate(BWAPI::Unit)
{
}

void ShushanKingAIModule::onUnitDestroy(BWAPI::Unit)
{
}

void ShushanKingAIModule::onUnitMorph(BWAPI::Unit)
{
}

void ShushanKingAIModule::onUnitRenegade(BWAPI::Unit)
{
}

void ShushanKingAIModule::onSaveGame(std::string)
{
}

void ShushanKingAIModule::onUnitComplete(BWAPI::Unit)
{
}
