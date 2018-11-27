#include "LocutusAIModule.h"

#include <chrono>

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

    auto start = std::chrono::high_resolution_clock::now();

    Map::initialize();
    BuildingPlacement::initialize();

    BWAPI::Broodwar->setLocalSpeed(0);
    BWAPI::Broodwar->setFrameSkip(0);

    Strategist::chooseOpening();

    Log::Debug() << "Startup time: " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();

    Log::Get() << "I am Locutus of Borg, you are " << Opponent::getName() << ", we're in " << BWAPI::Broodwar->mapFileName();
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

    auto start = std::chrono::high_resolution_clock::now();

    // Update general information things
    Units::update();
    Log::Debug() << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count() << ": Units::update";

    Workers::updateAssignments();
    Log::Debug() << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count() << ": Workers::updateAssignments";

    // Update stuff that issues orders
    Producer::update();
    Log::Debug() << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count() << ": Producer::update";

    Builder::update();
    Log::Debug() << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count() << ": Builder::update";

    Workers::issueOrders(); // Called last to allow workers to be taken or released for building, combat, etc. earlier
    Log::Debug() << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count() << ": Workers::issueOrders";
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
}

void LocutusAIModule::onUnitDestroy(BWAPI::Unit unit)
{
    Map::onUnitDestroy(unit);
    BuildingPlacement::onUnitDestroy(unit);
    Units::onUnitDestroy(unit);
    Workers::onUnitDestroy(unit);
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
