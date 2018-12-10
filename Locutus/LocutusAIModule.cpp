#include "LocutusAIModule.h"

#include "Timer.h"
#include "Map.h"
#include "Producer.h"
#include "Builder.h"
#include "BuildingPlacement.h"
#include "Opponent.h"
#include "Strategist.h"
#include "Units.h"
#include "Workers.h"

void LocutusAIModule::onStart()
{
    Log::SetDebug(true);

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
}

void LocutusAIModule::onFrame()
{
    if (BWAPI::Broodwar->getFrameCount() > 10000)
    {
        BWAPI::Broodwar->leaveGame();
        return;
    }

    Timer::start("Frame");

    // Update general information things
    Units::update();
    Timer::checkpoint("Units::update");

    Workers::updateAssignments();
    Timer::checkpoint("Workers::updateAssignments");

    BuildingPlacement::update();
    Timer::checkpoint("BuildingPlacement::update");

    Builder::update();
    Timer::checkpoint("Builder::update");

    // Update stuff that issues orders
    Producer::update();
    Timer::checkpoint("Producer::update");

    Builder::issueOrders();
    Timer::checkpoint("Builder::issueOrders");

    Workers::issueOrders(); // Called last to allow workers to be taken or released for building, combat, etc. earlier
    Timer::checkpoint("Workers::issueOrders");

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
    Map::onUnitCreate(unit);
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
}

void LocutusAIModule::onUnitRenegade(BWAPI::Unit unit)
{
    Units::onUnitRenegade(unit);
    Workers::onUnitRenegade(unit);
}

void LocutusAIModule::onSaveGame(std::string gameName)
{
}

void LocutusAIModule::onUnitComplete(BWAPI::Unit unit)
{
}
