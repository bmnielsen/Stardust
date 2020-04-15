#pragma once

#include "BWAPI.h"
#include <cassert>

namespace Config
{
    extern std::string StardustTestStrategyName;
    extern bool StardustTestForceGasSteal;

    namespace ConfigFile
    {
        extern bool ConfigFileFound;
        extern bool ConfigFileParsed;
        extern std::string ConfigFileLocation;
    }

    namespace BotInfo
    {
        extern std::string BotName;
        extern std::string Authors;
        extern bool PrintInfoOnStart;
    }

	namespace IO
	{
		extern std::string ErrorLogFilename;
		extern bool LogAssertToErrorFile;

        extern std::string StaticDir;
        extern std::string PreparedDataDir;
        extern std::string ReadDir;
		extern std::string WriteDir;
		extern int MaxGameRecords;
		extern bool ReadOpponentModel;
		extern bool WriteOpponentModel;
	}

    namespace Skills
    {
        extern bool UnderSCHNAIL;
        extern bool SCHNAILMeansHuman;
        extern bool HumanOpponent;
        extern bool SurrenderWhenHopeIsLost;

        extern bool ScoutHarassEnemy;
        extern bool AutoGasSteal;
        extern double RandomGasStealRate;

        extern bool Burrow;
        extern int MaxQueens;
        extern int MaxInfestedTerrans;
    }

	namespace Strategy
    {
        extern std::string StrategyName;
		extern bool UsePlanRecognizer;
        extern bool UseEnemySpecificStrategy;
        extern bool FoundEnemySpecificStrategy;
    }

    namespace BWAPIOptions
    {
        extern int SetLocalSpeed;
        extern int SetFrameSkip;
        extern bool EnableUserInput;
        extern bool EnableCompleteMapInformation;
    }

    namespace Tournament
    {
        extern int GameEndFrame;	
    }

    namespace Debug
    {
        extern bool DrawGameInfo;
        extern bool DrawUnitHealthBars;
		extern bool DrawProductionInfo;
		extern bool DrawBuildOrderSearchInfo;
		extern bool DrawQueueFixInfo;
		extern bool DrawScoutInfo;
		extern bool DrawEnemyUnitInfo;
		extern bool DrawHiddenEnemies;
		extern bool DrawModuleTimers;
		extern bool DrawResourceInfo;
		extern bool DrawCombatSimulationInfo;
		extern bool DrawUnitTargetInfo;
		extern bool DrawUnitOrders;
		extern bool DrawMicroState;
		extern bool DrawMapInfo;
		extern bool DrawMapGrid;
		extern bool DrawMapDistances;
		extern bool DrawBaseInfo;
		extern bool DrawStrategyBossInfo;
		extern bool DrawSquadInfo;
		extern bool DrawClusters;
		extern bool DrawWorkerInfo;
		extern bool DrawMouseCursorInfo;
        extern bool DrawBuildingInfo;
		extern bool DrawReservedBuildingTiles;
		extern bool DrawBOSSStateInfo;

        extern BWAPI::Color ColorLineTarget;
        extern BWAPI::Color ColorLineMineral;
        extern BWAPI::Color ColorUnitNearEnemy;
        extern BWAPI::Color ColorUnitNotNearEnemy;
    }

    namespace Micro
    {
        extern bool KiteWithRangedUnits;
        extern bool WorkersDefendRush;
        extern int RetreatMeleeUnitShields;
        extern int RetreatMeleeUnitHP;
        extern int CombatSimRadius;         
		extern int ScoutDefenseRadius;
	}
    
    namespace Macro
    {
        extern int BOSSFrameLimit;
        extern int WorkersPerRefinery;
		extern double WorkersPerPatch;
		extern int AbsoluteMaxWorkers;
        extern int BuildingSpacing;
        extern int PylonSpacing;
		extern int ProductionJamFrameLimit;
		extern bool ExpandToIslands;
    }

    namespace Tools
    {
        extern int MAP_GRID_SIZE;
    }
}