#include "LocutusAIModule.h"
#include "Map.h"
#include "BuildingPlacer.h"
#include "Opponent.h"
#include "Units.h"

void LocutusAIModule::onStart()
{
    Log::SetDebug(true);

    Map::initialize();
    BuildingPlacer::initialize();

    BWAPI::Broodwar->setLocalSpeed(0);
    BWAPI::Broodwar->setFrameSkip(0);

    // FIXME: Choose opening

    Log::Get() << "I am Locutus of Borg, you are " << Opponent::getName() << ", we're in " << BWAPI::Broodwar->mapFileName();
}

void LocutusAIModule::onEnd(bool isWinner)
{
}

void LocutusAIModule::onFrame()
{
    Units::update();
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
    BuildingPlacer::onUnitDiscover(unit);
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
    BuildingPlacer::onUnitCreate(unit);
}

void LocutusAIModule::onUnitDestroy(BWAPI::Unit unit)
{
    Map::onUnitDestroy(unit);
    BuildingPlacer::onUnitDestroy(unit);
    Units::onUnitDestroy(unit);
}

void LocutusAIModule::onUnitMorph(BWAPI::Unit unit)
{
    BuildingPlacer::onUnitMorph(unit);
}

void LocutusAIModule::onUnitRenegade(BWAPI::Unit unit)
{
    Units::onUnitRenegade(unit);
}

void LocutusAIModule::onSaveGame(std::string gameName)
{
}

void LocutusAIModule::onUnitComplete(BWAPI::Unit unit)
{
}
