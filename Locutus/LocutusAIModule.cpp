#include "LocutusAIModule.h"
#include "Map.h"
#include "Builder.h"
#include "BuildingPlacement.h"
#include "Opponent.h"
#include "Strategist.h"
#include "Units.h"
#include "Workers.h"

void LocutusAIModule::onStart()
{
    Log::SetDebug(true);

    Map::initialize();
    BuildingPlacement::initialize();

    BWAPI::Broodwar->setLocalSpeed(0);
    BWAPI::Broodwar->setFrameSkip(0);

    Strategist::chooseOpening();

    Log::Get() << "I am Locutus of Borg, you are " << Opponent::getName() << ", we're in " << BWAPI::Broodwar->mapFileName();
}

void LocutusAIModule::onEnd(bool isWinner)
{
}

void LocutusAIModule::onFrame()
{
    // Update general information things
    Units::update();

    // Update stuff that issues orders
    Builder::update();
    Workers::update(); // Called last to allow workers to be taken or released for building, combat, etc. earlier
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
