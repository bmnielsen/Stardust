#include "StardustAIModule.h"

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
#include "WorkerOrderTimer.h"
#include "Bullets.h"
#include "Players.h"
#include "Geo.h"

int currentFrame;

// While instrumenting we have a lower global frame limit to ensure we get data if the game locks up
#if INSTRUMENTATION_ENABLED_VERBOSE
#define FRAME_LIMIT 25000
#else
#if INSTRUMENTATION_ENABLED
#define FRAME_LIMIT 40000
#endif
#endif

#if INSTRUMENTATION_ENABLED_VERBOSE

// Heatmaps are quite large, so we don't always want to write them every frame
// These defines configure what frequency to dump them, or 0 to disable them
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

    void handleUnitDiscover(BWAPI::Unit unit)
    {
        Map::onUnitDiscover(unit);
    }

    void handleUnitDestroy(BWAPI::Unit unit)
    {
        Map::onUnitDestroy(unit);
        Units::onUnitDestroy(unit);
    }
}

void StardustAIModule::onStart()
{
    gameFinished = false;
    currentFrame = 0;

    // Initialize globals that just need to make sure their global data is reset
    Log::initialize();
    Builder::initialize();
    Opponent::initialize();
    General::initialize();
    Units::initialize();
    Workers::initialize();
    Bullets::initialize();
    Players::initialize();
    Geo::initialize();
    PathFinding::clearGrids();
    PathFinding::initializeSearch();

    Log::SetDebug(true);
    CherryVis::initialize();

    if (moduleBaseAddress > 0)
    {
        Log::Get() << "Module base address: 0x" << std::hex << moduleBaseAddress;
    }

    Timer::start("Startup");

    Map::initialize();
    Timer::checkpoint("Map::initialize");

    PathFinding::initializeGrids();
    Timer::checkpoint("PathFinding::initialize");

    BuildingPlacement::initialize();
    Timer::checkpoint("BuildingPlacement::initialize");

    CombatSim::initialize();
    Timer::checkpoint("CombatSim::initialize");

    WorkerOrderTimer::initialize();
    Timer::checkpoint("WorkerOrderTimer::initialize");

    Strategist::initialize();
    Timer::checkpoint("Strategist::initialize");

    Timer::stop(true);

    Log::Get() << "Initialized game against " << Opponent::getName()
               << " on " << BWAPI::Broodwar->mapFileName() << " (" << BWAPI::Broodwar->mapHash() << ")";
    Log::Get() << "My starting position: " << Map::getMyMain()->getTilePosition();
    if (Map::getMyNatural())
    {
        Log::Get() << "My natural position: " << Map::getMyNatural()->getTilePosition();
    }
    else
    {
        Log::Get() << "No natural position available";
    }
    if (Map::getMyMainChoke())
    {
        Log::Get() << "My main choke: " << BWAPI::TilePosition(Map::getMyMainChoke()->center);
    }
    else
    {
        Log::Get() << "No main choke available";
    }

    if (testOnStart) testOnStart();
}

void StardustAIModule::onEnd(bool isWinner)
{
    if (testOnEnd) testOnEnd(isWinner);

    Opponent::gameEnd(isWinner);
    WorkerOrderTimer::write();
    CherryVis::gameEnd();
}

void StardustAIModule::onFrame()
{
    if (BWAPI::Broodwar->getFrameCount() < frameSkip)
    {
        currentFrame++;
        return;
    }
    if (gameFinished) return;

    if (BWAPI::Broodwar->isPaused()) return;
    if (BWAPI::Broodwar->isReplay()) return;

#ifdef FRAME_LIMIT
    if (currentFrame > FRAME_LIMIT)
    {
        gameFinished = true;
        BWAPI::Broodwar->leaveGame();
        return;
    }
#endif

    Timer::start("Frame");

    // Before doing anything else, check if the opponent has left or been eliminated
    for (auto &event : BWAPI::Broodwar->getEvents())
    {
        if (event.getType() == BWAPI::EventType::PlayerLeft && event.getPlayer() == BWAPI::Broodwar->enemy()
            && currentFrame > 100)
        {
            Log::Get() << "Opponent has left the game";
            gameFinished = true;
            return;
        }
    }

    // First priority is to update unit-related things, as most of our other stuff relies on our unit abstraction being updated

    // We start with bullets though as they interact directly with units
    Bullets::update();
    Timer::checkpoint("Bullets::update");

    Units::update();
    Timer::checkpoint("Units::update");

    // We handle events explicitly instead of through the event handlers so we can time them
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
                Log::Get() << "Received text: " << event.getText();
                break;
            default:
                break;
        }
    }
    Timer::checkpoint("Events");

    // Update general information things
    Opponent::update();
    Timer::checkpoint("Opponent::update");

    Players::update();
    Timer::checkpoint("Players::update");

    WorkerOrderTimer::update();
    Timer::checkpoint("WorkerOrderTimer::update");

    Map::update();
    Timer::checkpoint("Map::update");

    General::updateClusters();
    Timer::checkpoint("General::updateClusters");

    BuildingPlacement::update();
    Timer::checkpoint("BuildingPlacement::update");

    Builder::update();
    Timer::checkpoint("Builder::update");

    Workers::updateAssignments();
    Timer::checkpoint("Workers::updateAssignments");

    // Strategist is what makes all of the big decisions
    Strategist::update();
    Timer::checkpoint("Strategist::update");

    // Hook our test infrastructure in here in case we want to have our units do different stuff
    if (testOnFrame) testOnFrame();

    // Update stuff that issues orders
    General::issueOrders();
    Timer::checkpoint("General::issueOrders");

    Producer::update();
    Timer::checkpoint("Producer::update");

    Builder::issueOrders();
    Timer::checkpoint("Builder::issueOrders");

    // Called after the above to allow workers to be reassigned for combat, scouting, and building first
    Workers::issueOrders();
    Timer::checkpoint("Workers::issueOrders");

    // Must be last, as this is what executes move orders queued earlier
    Units::issueOrders();
    Timer::checkpoint("Units::issueOrders");

#if LOGGING_ENABLED
    auto enemyNatural = Map::getEnemyStartingNatural();
    if (enemyNatural && enemyNatural->resourceDepot && enemyNatural->resourceDepot->bwapiUnit->isVisible()
        && enemyNatural->resourceDepot->health > 100)
    {
        // Check for other visible enemy units
        std::set<Unit> enemyUnits;
        Units::enemyInRadius(enemyUnits, enemyNatural->getPosition(), 640, [](const Unit &unit){ return !unit->type.isBuilding() && unit->bwapiUnit->isVisible();});
        if (enemyUnits.empty())
        {
            int dragoons = 0;
            for (auto &unit : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Dragoon))
            {
                if (unit->cooldownUntil > currentFrame) continue;
                if (unit->isInOurWeaponRange(enemyNatural->resourceDepot)) dragoons++;
            }

            if (dragoons > 3)
            {
                Log::Get() << "ERROR: Multiple dragoons aren't attacking lone enemy natural!";
            }
        }
    }
#endif

    // Surrender logic
    if (enableSurrender)
    {
        // Surrender if the following is true:
        // - We have no workers left and no money / depot to create a new one
        // - We have no mobile combat units
        // - There is at least one enemy combat unit in our main base
        if (Units::countAll(BWAPI::UnitTypes::Protoss_Probe) == 0 &&
            (Units::countAll(BWAPI::UnitTypes::Protoss_Nexus) == 0 || BWAPI::Broodwar->self()->minerals() < 50))
        {
            bool weHaveAMobileCombatUnit = false;
            for (auto &unit : Units::allMine())
            {
                if (!unit->canAttackGround()) continue;
                if (!unit->type.canMove()) continue;
                weHaveAMobileCombatUnit = true;
                break;
            }

            if (!weHaveAMobileCombatUnit)
            {
                bool enemyInBase = false;
                for (auto &base : Map::getMyBases())
                {
                    if (enemyInBase) break;
                    for (auto &unit : Units::enemyAtBase(base))
                    {
                        if (!unit->canAttackGround()) continue;
                        if (unit->type.isWorker()) continue;
                        enemyInBase = true;
                        break;
                    }
                }

                if (enemyInBase)
                {
                    Log::Get() << "Surrendering";
                    BWAPI::Broodwar->sendText("gg");
                    gameFinished = true;
                    BWAPI::Broodwar->leaveGame();
                }
            }
        }
    }

    // Instrumentation
    NoGoAreas::writeInstrumentation();
    General::writeInstrumentation();
    WorkerOrderTimer::writeInstrumentation();

#if COLLISION_HEATMAP_FREQUENCY_ENEMY
    if (currentFrame % COLLISION_HEATMAP_FREQUENCY_ENEMY == 0)
    {
        Players::grid(BWAPI::Broodwar->enemy()).dumpCollisionHeatmapIfChanged("CollisionEnemy");
    }
#endif
#if COLLISION_HEATMAP_FREQUENCY_MINE
    if (currentFrame % COLLISION_HEATMAP_FREQUENCY_MINE == 0)
    {
        Players::grid(BWAPI::Broodwar->self()).dumpCollisionHeatmapIfChanged("CollisionMine");
    }
#endif
#if GROUND_THREAT_HEATMAP_FREQUENCY_ENEMY
    if (currentFrame % GROUND_THREAT_HEATMAP_FREQUENCY_ENEMY == 0)
    {
        Players::grid(BWAPI::Broodwar->enemy()).dumpGroundThreatHeatmapIfChanged("GroundThreatEnemy");
    }
#endif
#if GROUND_THREAT_HEATMAP_FREQUENCY_MINE
    if (currentFrame % GROUND_THREAT_HEATMAP_FREQUENCY_MINE == 0)
    {
        Players::grid(BWAPI::Broodwar->self()).dumpGroundThreatHeatmapIfChanged("GroundThreatMine");
    }
#endif
#if GROUND_THREAT_STATIC_HEATMAP_FREQUENCY_ENEMY
    if (currentFrame % GROUND_THREAT_STATIC_HEATMAP_FREQUENCY_ENEMY == 0)
    {
        Players::grid(BWAPI::Broodwar->enemy()).dumpStaticGroundThreatHeatmapIfChanged("GroundThreatStaticEnemy");
    }
#endif
#if GROUND_THREAT_STATIC_HEATMAP_FREQUENCY_MINE
    if (currentFrame % GROUND_THREAT_STATIC_HEATMAP_FREQUENCY_MINE == 0)
    {
        Players::grid(BWAPI::Broodwar->self()).dumpStaticGroundThreatHeatmapIfChanged("GroundThreatStaticMine");
    }
#endif
#if AIR_THREAT_HEATMAP_FREQUENCY_ENEMY
    if (currentFrame % AIR_THREAT_HEATMAP_FREQUENCY_ENEMY == 0)
    {
        Players::grid(BWAPI::Broodwar->enemy()).dumpAirThreatHeatmapIfChanged("AirThreatEnemy");
    }
#endif
#if AIR_THREAT_HEATMAP_FREQUENCY_MINE
    if (currentFrame % AIR_THREAT_HEATMAP_FREQUENCY_MINE == 0)
    {
        Players::grid(BWAPI::Broodwar->self()).dumpAirThreatHeatmapIfChanged("AirThreatMine");
    }
#endif
#if DETECTION_HEATMAP_FREQUENCY_ENEMY
    if (currentFrame % DETECTION_HEATMAP_FREQUENCY_ENEMY == 0)
    {
        Players::grid(BWAPI::Broodwar->enemy()).dumpDetectionHeatmapIfChanged("DetectionEnemy");
    }
#endif
#if DETECTION_HEATMAP_FREQUENCY_MINE
    if (currentFrame % DETECTION_HEATMAP_FREQUENCY_MINE == 0)
    {
        Players::grid(BWAPI::Broodwar->self()).dumpDetectionHeatmapIfChanged("DetectionMine");
    }
#endif
#if STASIS_RANGE_HEATMAP_FREQUENCY_ENEMY
    if (currentFrame % STASIS_RANGE_HEATMAP_FREQUENCY_ENEMY == 0)
    {
        Players::grid(BWAPI::Broodwar->enemy()).dumpStasisRangeHeatmapIfChanged("StasisRange");
    }
#endif
#if POWER_HEATMAP_FREQUENCY
    if (currentFrame % POWER_HEATMAP_FREQUENCY == 0)
    {
        Map::dumpPowerHeatmap();
    }
#endif
    CherryVis::frameEnd(currentFrame);
    Timer::checkpoint("Instrumentation");

    Timer::stop();

    currentFrame++;
}

void StardustAIModule::onSendText(std::string)
{
}

void StardustAIModule::onReceiveText(BWAPI::Player, std::string)
{
}

void StardustAIModule::onPlayerLeft(BWAPI::Player)
{
}

void StardustAIModule::onNukeDetect(BWAPI::Position)
{
}

void StardustAIModule::onUnitDiscover(BWAPI::Unit)
{
}

void StardustAIModule::onUnitEvade(BWAPI::Unit)
{
}

void StardustAIModule::onUnitShow(BWAPI::Unit)
{
}

void StardustAIModule::onUnitHide(BWAPI::Unit)
{
}

void StardustAIModule::onUnitCreate(BWAPI::Unit)
{
}

void StardustAIModule::onUnitDestroy(BWAPI::Unit)
{
}

void StardustAIModule::onUnitMorph(BWAPI::Unit)
{
}

void StardustAIModule::onUnitRenegade(BWAPI::Unit)
{
}

void StardustAIModule::onSaveGame(std::string)
{
}

void StardustAIModule::onUnitComplete(BWAPI::Unit)
{
}
