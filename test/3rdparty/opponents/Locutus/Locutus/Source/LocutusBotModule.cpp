/* 
 *----------------------------------------------------------------------
 * Locutus entry point.
 *----------------------------------------------------------------------
 */

#include "LocutusBotModule.h"

#include "Bases.h"
#include "Common.h"
#include "ParseUtils.h"
#include "WorkerOrderTimer.h"
#include <filesystem>

#include "GameCommander.h"
#include <iostream>
#include "Logger.h"
#include "MapTools.h"
#include "Config.h"

#include "rapidjson/document.h"


using namespace Locutus;

namespace { auto & bwemMap = BWEM::Map::Instance(); }
namespace { auto & bwebMap = BWEB::Map::Instance(); }

bool gameEnded;

// This gets called when the bot starts.
void LocutusBotModule::onStart()
{
    gameEnded = false;

    // Uncomment this when we need to debug log stuff before the config file is parsed
    //Config::Debug::LogDebug = true;

    // Initialize BOSS, the Build Order Search System
    BOSS::init();

	// Call BWTA to read the current map.
	// Fails if we don't have the cache data.
	if (!BWTA::analyze())
	{
		UAB_ASSERT(false, "BWTA map analysis failed");
		Log().Get() << "BWTA map analysis failed for " << BWAPI::Broodwar->mapFileName() << " (" << BWAPI::Broodwar->mapHash() << ") - is the BWTA cache file present?";
		gameEnded = true;
		return;
	}

	// BWEM map init
	bwemMap.Initialize(BWAPI::BroodwarPtr);
	bwemMap.EnableAutomaticPathAnalysis();
	bool startingLocationsOK = bwemMap.FindBasesForStartingLocations();
	UAB_ASSERT(startingLocationsOK, "BWEM map analysis failed");

    // BWEB map init
    BuildingPlacer::Instance().initializeBWEB();

    // Our own map analysis.
    Bases::Instance().initialize();

    // Parse the bot's configuration file.
	// Change this file path to point to your config file.
    // Any relative path name will be relative to Starcraft installation folder
	// The config depends on the map and must be read after the map is analyzed.
	if (std::filesystem::exists("bwapi-data/read/Locutus.json"))
	{
		ParseUtils::ParseConfigFile("bwapi-data/read/Locutus.json");
	}
	else 
	{
		ParseUtils::ParseConfigFile(Config::ConfigFile::ConfigFileLocation);
	}

    // Set our BWAPI options according to the configuration.
	BWAPI::Broodwar->setLocalSpeed(Config::BWAPIOptions::SetLocalSpeed);
	BWAPI::Broodwar->setFrameSkip(Config::BWAPIOptions::SetFrameSkip);
    
    if (Config::BWAPIOptions::EnableCompleteMapInformation)
    {
        BWAPI::Broodwar->enableFlag(BWAPI::Flag::CompleteMapInformation);
    }

    if (Config::BWAPIOptions::EnableUserInput)
    {
        BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);
    }

    WorkerOrderTimer::initialize();

	Log().Get() << "I am Locutus of Borg, you are " << InformationManager::Instance().getEnemyName() << ", we're in " << BWAPI::Broodwar->mapFileName() << " (" << BWAPI::Broodwar->mapHash() << ")";

	StrategyManager::Instance().initializeOpening();    // may depend on config and/or opponent model

    if (Config::BotInfo::PrintInfoOnStart)
    {
        //BWAPI::Broodwar->printf("%s by %s, based on Locutus via Locutus.", Config::BotInfo::BotName.c_str(), Config::BotInfo::Authors.c_str());
	}
}

void LocutusBotModule::setStrategy(std::string strategy)
{
    Config::StardustTestStrategyName = strategy;
}

void LocutusBotModule::forceGasSteal()
{
    Config::StardustTestForceGasSteal = true;
}

std::string LocutusBotModule::getStrategyName()
{
    return Config::Strategy::StrategyName;
}

void LocutusBotModule::onEnd(bool isWinner)
{
    if (gameEnded) return;

    GameCommander::Instance().onEnd(isWinner);

    WorkerOrderTimer::write();

    gameEnded = true;
}

void LocutusBotModule::onFrame()
{
    if (gameEnded) return;

    if (!Config::ConfigFile::ConfigFileFound)
    {
        BWAPI::Broodwar->drawBoxScreen(0,0,450,100, BWAPI::Colors::Black, true);
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Huge);
        //BWAPI::Broodwar->drawTextScreen(10, 5, "%c%s Config File Not Found", red, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Default);
        //BWAPI::Broodwar->drawTextScreen(10, 30, "%c%s will not run without its configuration file", white, Config::BotInfo::BotName.c_str());
        //BWAPI::Broodwar->drawTextScreen(10, 45, "%cCheck that the file below exists. Incomplete paths are relative to Starcraft directory", white);
        //BWAPI::Broodwar->drawTextScreen(10, 60, "%cYou can change this file location in Config::ConfigFile::ConfigFileLocation", white);
        //BWAPI::Broodwar->drawTextScreen(10, 75, "%cFile Not Found (or is empty): %c %s", white, green, Config::ConfigFile::ConfigFileLocation.c_str());
        return;
    }
    else if (!Config::ConfigFile::ConfigFileParsed)
    {
        BWAPI::Broodwar->drawBoxScreen(0,0,450,100, BWAPI::Colors::Black, true);
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Huge);
        //BWAPI::Broodwar->drawTextScreen(10, 5, "%c%s Config File Parse Error", red, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Default);
        //BWAPI::Broodwar->drawTextScreen(10, 30, "%c%s will not run without a properly formatted configuration file", white, Config::BotInfo::BotName.c_str());
        //BWAPI::Broodwar->drawTextScreen(10, 45, "%cThe configuration file was found, but could not be parsed. Check that it is valid JSON", white);
        //BWAPI::Broodwar->drawTextScreen(10, 60, "%cFile Not Parsed: %c %s", white, green, Config::ConfigFile::ConfigFileLocation.c_str());
        return;
    }

	GameCommander::Instance().update();
}

void LocutusBotModule::onUnitDestroy(BWAPI::Unit unit)
{
    if (gameEnded) return;

    if (unit->getType().isMineralField())
		bwemMap.OnMineralDestroyed(unit);
	else if (unit->getType().isSpecialBuilding())
		bwemMap.OnStaticBuildingDestroyed(unit);

	bwebMap.onUnitDestroy(unit);

	GameCommander::Instance().onUnitDestroy(unit);
}

void LocutusBotModule::onUnitMorph(BWAPI::Unit unit)
{
    if (gameEnded) return;

    bwebMap.onUnitMorph(unit);

	GameCommander::Instance().onUnitMorph(unit);
}

void LocutusBotModule::onSendText(std::string text) 
{ 
    if (gameEnded) return;

	ParseUtils::ParseTextCommand(text);
}

void LocutusBotModule::onUnitCreate(BWAPI::Unit unit)
{ 
    if (gameEnded) return;

    bwebMap.onUnitDiscover(unit);

	GameCommander::Instance().onUnitCreate(unit);
}

void LocutusBotModule::onUnitDiscover(BWAPI::Unit unit)
{ 
    if (gameEnded) return;

    bwebMap.onUnitDiscover(unit);
}

void LocutusBotModule::onUnitComplete(BWAPI::Unit unit)
{
    if (gameEnded) return;

    GameCommander::Instance().onUnitComplete(unit);
}

void LocutusBotModule::onUnitShow(BWAPI::Unit unit)
{ 
    if (gameEnded) return;

    GameCommander::Instance().onUnitShow(unit);
}

void LocutusBotModule::onUnitHide(BWAPI::Unit unit)
{ 
    if (gameEnded) return;

    GameCommander::Instance().onUnitHide(unit);
}

void LocutusBotModule::onUnitRenegade(BWAPI::Unit unit)
{ 
    if (gameEnded) return;

	GameCommander::Instance().onUnitRenegade(unit);
}
