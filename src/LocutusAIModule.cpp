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
#include "Producer.h"
#include "Builder.h"
#include "BuildingPlacement.h"
#include "Opponent.h"
#include "Strategist.h"
#include "General.h"
#include "Units.h"
#include "Workers.h"
#include "Scout.h"
#include "Bullets.h"
#include "Players.h"

void LocutusAIModule::onStart()
{
    Log::SetDebug(true);
    CherryVis::initialize();

    Timer::start("Startup");

    Map::initialize();
    BuildingPlacement::initialize();

    BWAPI::Broodwar->setLocalSpeed(0);
    BWAPI::Broodwar->setFrameSkip(0);

    Strategist::chooseOpening();

    Timer::stop();

    Log::Get() << "I am Locutus of Borg, you are " << Opponent::getName() << ", we're in " << BWAPI::Broodwar->mapFileName();
    //Log::Debug() << "Seed: " << BWAPI::Broodwar->getRandomSeed();
}

void LocutusAIModule::onEnd(bool isWinner)
{
    CherryVis::gameEnd();
}

void LocutusAIModule::onFrame()
{
    if (BWAPI::Broodwar->getFrameCount() > 30000)
    {
        BWAPI::Broodwar->leaveGame();
        return;
    }

    Timer::start("Frame");

    // Update general information things
    Players::update();
    Timer::checkpoint("Players::update");

    Bullets::update();
    Timer::checkpoint("Bullets::update");

    Units::update();
    Timer::checkpoint("Units::update");

    Map::update();
    Timer::checkpoint("Map::update");

    General::updateAssignments();
    Timer::checkpoint("General::updateAssignments");

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

    Workers::issueOrders(); // Called last to allow workers to be taken or released for building, combat, etc. earlier
    Timer::checkpoint("Workers::issueOrders");

    // Instrumentation
#ifdef COLLISION_HEATMAP_FREQUENCY
    if (BWAPI::Broodwar->getFrameCount() % COLLISION_HEATMAP_FREQUENCY == 0)
    {
        Players::grid(BWAPI::Broodwar->self()).dumpCollisionHeatmapIfChanged("Collision");
    }
#endif
#ifdef GROUND_THREAT_HEATMAP_FREQUENCY
    if (BWAPI::Broodwar->getFrameCount() % GROUND_THREAT_HEATMAP_FREQUENCY == 0)
    {
        Players::grid(BWAPI::Broodwar->enemy()).dumpGroundThreatHeatmapIfChanged("GroundThreat");
    }
#endif
#ifdef AIR_THREAT_HEATMAP_FREQUENCY
    if (BWAPI::Broodwar->getFrameCount() % AIR_THREAT_HEATMAP_FREQUENCY == 0)
    {
        Players::grid(BWAPI::Broodwar->enemy()).dumpAirThreatHeatmapIfChanged("AirThreat");
    }
#endif
#ifdef DETECTION_HEATMAP_FREQUENCY
    if (BWAPI::Broodwar->getFrameCount() % DETECTION_HEATMAP_FREQUENCY == 0)
    {
        Players::grid(BWAPI::Broodwar->enemy()).dumpDetectionHeatmapIfChanged("Detection");
    }
#endif
#ifdef VISIBILITY_HEATMAP_FREQUENCY
    if (BWAPI::Broodwar->getFrameCount() % VISIBILITY_HEATMAP_FREQUENCY == 0)
    {
        Map::dumpVisibilityHeatmap();
    }
#endif
    CherryVis::frameEnd(BWAPI::Broodwar->getFrameCount());
    Timer::checkpoint("Instrumentation");

    Timer::stop();
}

void LocutusAIModule::onSendText(std::string text)
{
}

void LocutusAIModule::onReceiveText(BWAPI::Player player, std::string text)
{
}

void LocutusAIModule::onPlayerLeft(BWAPI::Player player)
{
}

void LocutusAIModule::onNukeDetect(BWAPI::Position target)
{
}

void LocutusAIModule::onUnitDiscover(BWAPI::Unit unit)
{
    BuildingPlacement::onUnitDiscover(unit);
    Map::onUnitDiscover(unit);
}

void LocutusAIModule::onUnitEvade(BWAPI::Unit unit)
{
}

void LocutusAIModule::onUnitShow(BWAPI::Unit unit)
{
}

void LocutusAIModule::onUnitHide(BWAPI::Unit unit)
{
}

void LocutusAIModule::onUnitCreate(BWAPI::Unit unit)
{
    BuildingPlacement::onUnitCreate(unit);

    if (unit->getPlayer() == BWAPI::Broodwar->self())
    {
        Log::Get() << "Unit created: " << unit->getType() << " @ " << unit->getTilePosition();
    }
}

void LocutusAIModule::onUnitDestroy(BWAPI::Unit unit)
{
    Map::onUnitDestroy(unit);
    BuildingPlacement::onUnitDestroy(unit);
    Units::onUnitDestroy(unit);
    Workers::onUnitDestroy(unit);

    if (unit->getPlayer() == BWAPI::Broodwar->self())
    {
        Log::Get() << "Unit lost: " << unit->getType() << " @ " << unit->getTilePosition();
    }
}

void LocutusAIModule::onUnitMorph(BWAPI::Unit unit)
{
    BuildingPlacement::onUnitMorph(unit);

    if (unit->getPlayer() == BWAPI::Broodwar->self() && unit->getType().isRefinery())
    {
        Log::Get() << "Unit created: " << unit->getType() << " @ " << unit->getTilePosition();
    }
}

void LocutusAIModule::onUnitRenegade(BWAPI::Unit unit)
{
    Workers::onUnitRenegade(unit);
}

void LocutusAIModule::onSaveGame(std::string gameName)
{
}

void LocutusAIModule::onUnitComplete(BWAPI::Unit unit)
{
}