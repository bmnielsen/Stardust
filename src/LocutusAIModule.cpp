/*
TODOs:
- Ability to take expansions
- Ability to build static defense / walls
- Ability to proxy
- Scouting
- Combat
- Flocking
- Threat-aware pathfinding
- Path smoothing? (test with scouting probe to see how big an impact it has)
  - e.g. cache most efficient paths between common waypoints like narrow chokes, invalidate when something blocks them
  - brushfire to common destinations, e.g. each base or choke, maybe recalculate for changes

Tasks:
- Get simple combat management working
  - Including combat sim with customizations
- Expansions, static defense, walls
- Enhanced scouting with opponent plan recognition
- Worker defense
- Start looking into pathfinding stuff
- Enhanced combat mechanics:
  - Avoiding overkill
  - Army movement
  - Avoid narrow chokes where possible
- Proxies
- Worker harrass
*/


#include "LocutusAIModule.h"

#include "Timer.h"
#include "Map.h"
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
#include "Scout.h"
#include "Bullets.h"
#include "Players.h"

#if INSTRUMENTATION_ENABLED
    // While instrumenting we have a global frame limit to ensure we get data if the game locks up
    #define FRAME_LIMIT 30000

    // Heatmaps are quite large, so we don't always want to write them every frame
    // These defines configure what frequency to dump them, or 0 to disable them
    #define COLLISION_HEATMAP_FREQUENCY_ENEMY 0
    #define GROUND_THREAT_HEATMAP_FREQUENCY_ENEMY 0
    #define AIR_THREAT_HEATMAP_FREQUENCY_ENEMY 0
    #define DETECTION_HEATMAP_FREQUENCY_ENEMY 0

    #define COLLISION_HEATMAP_FREQUENCY_MINE 0
    #define GROUND_THREAT_HEATMAP_FREQUENCY_MINE 0
    #define AIR_THREAT_HEATMAP_FREQUENCY_MINE 0
    #define DETECTION_HEATMAP_FREQUENCY_MINE 0

    #define VISIBILITY_HEATMAP_FREQUENCY 0 // Only tracked for self
#endif

namespace
{
    void handleUnitDiscover(BWAPI::Unit unit)
    {
        Map::onUnitDiscover(unit);
    }

    void handleUnitDestroy(BWAPI::Unit unit)
    {
        Map::onUnitDestroy(unit);
        Units::onUnitDestroy(unit);
        Workers::onUnitDestroy(unit);
    }
}

void LocutusAIModule::onStart()
{
    // Initialize globals that just need to make sure their global data is reset
    Log::initialize();
    Builder::initialize();
    Opponent::initialize();
    General::initialize();
    Units::initialize();
    Workers::initialize();
    Bullets::initialize();
    Scout::initialize();
    Players::initialize();

    Log::SetDebug(true);
    CherryVis::initialize();

    Timer::start("Startup");

    Map::initialize();
    Timer::checkpoint("Map::initialize");

    PathFinding::initialize();
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

    BWAPI::Broodwar->setLocalSpeed(0);
    BWAPI::Broodwar->setFrameSkip(0);

    Log::Get() << "I am Locutus of Borg, you are " << Opponent::getName() << ", we're in " << BWAPI::Broodwar->mapFileName();
    //Log::Debug() << "Seed: " << BWAPI::Broodwar->getRandomSeed();
}

void LocutusAIModule::onEnd(bool isWinner)
{
    WorkerOrderTimer::write();
    CherryVis::gameEnd();
}

void LocutusAIModule::onFrame()
{
    if (BWAPI::BroodwarPtr->getFrameCount() < frameSkip) return;
    if (gameFinished) return;

#ifdef FRAME_LIMIT
    if (BWAPI::Broodwar->getFrameCount() > FRAME_LIMIT)
    {
        BWAPI::Broodwar->leaveGame();
        return;
    }
#endif

    Timer::start("Frame");

    // Before doing anything else, check if the opponent has left or been eliminated
    for (auto &event : BWAPI::Broodwar->getEvents())
    {
        if (event.getType() == BWAPI::EventType::PlayerLeft && event.getPlayer() == BWAPI::Broodwar->enemy())
        {
            Log::Get() << "Opponent has left the game";
            BWAPI::Broodwar->sendText("gg");
            gameFinished = true;
            return;
        }
    }

    // We update units as the first thing, since we want to use our own abstraction over BWAPI::Unit everywhere
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
    Players::update();
    Timer::checkpoint("Players::update");

    Bullets::update();
    Timer::checkpoint("Bullets::update");

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

    // Update stuff that issues orders
    General::issueOrders();
    Timer::checkpoint("General::issueOrders");

    Scout::update();
    Timer::checkpoint("Scout::update");

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

    // Instrumentation
#if COLLISION_HEATMAP_FREQUENCY_ENEMY
    if (BWAPI::Broodwar->getFrameCount() % COLLISION_HEATMAP_FREQUENCY_ENEMY == 0)
    {
        Players::grid(BWAPI::Broodwar->enemy()).dumpCollisionHeatmapIfChanged("CollisionEnemy");
    }
#endif
#if COLLISION_HEATMAP_FREQUENCY_MINE
    if (BWAPI::Broodwar->getFrameCount() % COLLISION_HEATMAP_FREQUENCY_MINE == 0)
    {
        Players::grid(BWAPI::Broodwar->self()).dumpCollisionHeatmapIfChanged("CollisionMine");
    }
#endif
#if GROUND_THREAT_HEATMAP_FREQUENCY_ENEMY
    if (BWAPI::Broodwar->getFrameCount() % GROUND_THREAT_HEATMAP_FREQUENCY_ENEMY == 0)
    {
        Players::grid(BWAPI::Broodwar->enemy()).dumpGroundThreatHeatmapIfChanged("GroundThreatEnemy");
    }
#endif
#if GROUND_THREAT_HEATMAP_FREQUENCY_MINE
    if (BWAPI::Broodwar->getFrameCount() % GROUND_THREAT_HEATMAP_FREQUENCY_MINE == 0)
    {
        Players::grid(BWAPI::Broodwar->self()).dumpGroundThreatHeatmapIfChanged("GroundThreatMine");
    }
#endif
#if AIR_THREAT_HEATMAP_FREQUENCY_ENEMY
    if (BWAPI::Broodwar->getFrameCount() % AIR_THREAT_HEATMAP_FREQUENCY_ENEMY == 0)
    {
        Players::grid(BWAPI::Broodwar->enemy()).dumpAirThreatHeatmapIfChanged("AirThreatEnemy");
    }
#endif
#if AIR_THREAT_HEATMAP_FREQUENCY_MINE
    if (BWAPI::Broodwar->getFrameCount() % AIR_THREAT_HEATMAP_FREQUENCY_MINE == 0)
    {
        Players::grid(BWAPI::Broodwar->self()).dumpAirThreatHeatmapIfChanged("AirThreatMine");
    }
#endif
#if DETECTION_HEATMAP_FREQUENCY_ENEMY
    if (BWAPI::Broodwar->getFrameCount() % DETECTION_HEATMAP_FREQUENCY_ENEMY == 0)
    {
        Players::grid(BWAPI::Broodwar->enemy()).dumpDetectionHeatmapIfChanged("DetectionEnemy");
    }
#endif
#if DETECTION_HEATMAP_FREQUENCY_MINE
    if (BWAPI::Broodwar->getFrameCount() % DETECTION_HEATMAP_FREQUENCY_MINE == 0)
    {
        Players::grid(BWAPI::Broodwar->self()).dumpDetectionHeatmapIfChanged("DetectionMine");
    }
#endif
#if VISIBILITY_HEATMAP_FREQUENCY
    if (BWAPI::Broodwar->getFrameCount() % VISIBILITY_HEATMAP_FREQUENCY == 0)
    {
        Map::dumpVisibilityHeatmap();
    }
#endif
    CherryVis::frameEnd(BWAPI::Broodwar->getFrameCount());
    Timer::checkpoint("Instrumentation");

    Timer::stop();
}

void LocutusAIModule::onSendText(std::string)
{
}

void LocutusAIModule::onReceiveText(BWAPI::Player, std::string)
{
}

void LocutusAIModule::onPlayerLeft(BWAPI::Player)
{
}

void LocutusAIModule::onNukeDetect(BWAPI::Position)
{
}

void LocutusAIModule::onUnitDiscover(BWAPI::Unit)
{
}

void LocutusAIModule::onUnitEvade(BWAPI::Unit)
{
}

void LocutusAIModule::onUnitShow(BWAPI::Unit)
{
}

void LocutusAIModule::onUnitHide(BWAPI::Unit)
{
}

void LocutusAIModule::onUnitCreate(BWAPI::Unit)
{
}

void LocutusAIModule::onUnitDestroy(BWAPI::Unit)
{
}

void LocutusAIModule::onUnitMorph(BWAPI::Unit)
{
}

void LocutusAIModule::onUnitRenegade(BWAPI::Unit)
{
}

void LocutusAIModule::onSaveGame(std::string)
{
}

void LocutusAIModule::onUnitComplete(BWAPI::Unit)
{
}
