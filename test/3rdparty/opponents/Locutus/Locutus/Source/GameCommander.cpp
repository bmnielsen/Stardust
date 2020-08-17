#include "Common.h"
#include "GameCommander.h"
#include "OpponentModel.h"
#include "UnitUtil.h"
#include "PathFinding.h"

using namespace Locutus;

//#define CRASH_DEBUG 1

GameCommander::GameCommander() 
	: _combatCommander(CombatCommander::Instance())
	, _initialScoutTime(0)
	, _surrenderTime(0)
{
}

void GameCommander::update()
{
	_timerManager.startTimer(TimerManager::Total);

#ifdef CRASH_DEBUG
	Log().Debug() << "handleUnitAssignments";
#endif

	// populate the unit vectors we will pass into various managers
	handleUnitAssignments();

#ifdef CRASH_DEBUG
	Log().Debug() << "surrenderMonkey";
#endif

	// Decide whether to give up early. Implements config option SurrenderWhenHopeIsLost.
	if (surrenderMonkey()/* || BWAPI::Broodwar->getFrameCount() == 30000*/)
	{
		_surrenderTime = BWAPI::Broodwar->getFrameCount();
		//BWAPI::Broodwar->printf("gg");
	}
	if (_surrenderTime)
	{
		if (BWAPI::Broodwar->getFrameCount() - _surrenderTime >= 36)  // 36 frames = 1.5 game seconds
		{
			Log().Get() << "Surrendering";
			BWAPI::Broodwar->leaveGame();
		}
		return;
	}

#ifdef CRASH_DEBUG
	Log().Debug() << "InformationManager";
#endif

	// utility managers
	_timerManager.startTimer(TimerManager::InformationManager);
	InformationManager::Instance().update();
	_timerManager.stopTimer(TimerManager::InformationManager);

#ifdef CRASH_DEBUG
	Log().Debug() << "MapGrid";
#endif

	_timerManager.startTimer(TimerManager::MapGrid);
	MapGrid::Instance().update();
	_timerManager.stopTimer(TimerManager::MapGrid);

#ifdef CRASH_DEBUG
	Log().Debug() << "BOSSManager";
#endif

	_timerManager.startTimer(TimerManager::Search);
	BOSSManager::Instance().update(35 - _timerManager.getMilliseconds());
	_timerManager.stopTimer(TimerManager::Search);

#ifdef CRASH_DEBUG
	Log().Debug() << "WorkerManager";
#endif

	_timerManager.startTimer(TimerManager::Worker);
	WorkerManager::Instance().update();
	_timerManager.stopTimer(TimerManager::Worker);

#ifdef CRASH_DEBUG
	Log().Debug() << "StrategyManager";
#endif

	_timerManager.startTimer(TimerManager::Strategy);
    StrategyManager::Instance().update();
	_timerManager.stopTimer(TimerManager::Strategy);

#ifdef CRASH_DEBUG
	Log().Debug() << "ProductionManager";
#endif

	_timerManager.startTimer(TimerManager::Production);
	ProductionManager::Instance().update();
	_timerManager.stopTimer(TimerManager::Production);

#ifdef CRASH_DEBUG
	Log().Debug() << "BuildingManager";
#endif

	_timerManager.startTimer(TimerManager::Building);
	BuildingManager::Instance().update();
	_timerManager.stopTimer(TimerManager::Building);

#ifdef CRASH_DEBUG
	Log().Debug() << "_combatCommander";
#endif

	_timerManager.startTimer(TimerManager::Combat);
	_combatCommander.update(_combatUnits);
	_timerManager.stopTimer(TimerManager::Combat);

#ifdef CRASH_DEBUG
	Log().Debug() << "ScoutManager";
#endif

	_timerManager.startTimer(TimerManager::Scout);
    ScoutManager::Instance().update();
	_timerManager.stopTimer(TimerManager::Scout);

#ifdef CRASH_DEBUG
	Log().Debug() << "OpponentModel";
#endif

	_timerManager.startTimer(TimerManager::OpponentModel);
	OpponentModel::Instance().update();
	_timerManager.stopTimer(TimerManager::OpponentModel);

#ifdef CRASH_DEBUG
	Log().Debug() << "(done frame)";
#endif

	_timerManager.stopTimer(TimerManager::Total);

	if (_timerManager.getMilliseconds() > 45)
	{
        _timerManager.log();
	}

	drawDebugInterface();

    if (BWAPI::Broodwar->getFrameCount() % 2000 == 1999)
    {
        int count = 0;
        for (const auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (UnitUtil::IsCombatUnit(unit) && unit->isCompleted())
            {
                ++count;
            }
        }

        Log().Get() << "Summary: " << count << " combat units, " << UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Probe) << " workers, " << BWAPI::Broodwar->self()->minerals() << " minerals, " << BWAPI::Broodwar->self()->gas() << " gas";
    }
}

void GameCommander::drawDebugInterface()
{

}

void GameCommander::drawGameInformation(int x, int y)
{
}

void GameCommander::drawUnitOrders()
{
}

// assigns units to various managers
void GameCommander::handleUnitAssignments()
{
	_validUnits.clear();
    _combatUnits.clear();
	// Don't clear the scout units.

	// filter our units for those which are valid and usable
	setValidUnits();

	// set each type of unit
	setScoutUnits();
	setCombatUnits();
}

bool GameCommander::isAssigned(BWAPI::Unit unit) const
{
	return _combatUnits.contains(unit) || _scoutUnits.contains(unit);
}

// validates units as usable for distribution to various managers
void GameCommander::setValidUnits()
{
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (UnitUtil::IsValidUnit(unit))
		{	
			_validUnits.insert(unit);
		}
	}
}

void GameCommander::setScoutUnits()
{
	// If we're zerg, assign the first overlord to scout.
	// But not if the enemy is terran: We have no evasion skills, we'll lose the overlord.
	if (BWAPI::Broodwar->getFrameCount() == 0 &&
		BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg &&
		BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Terran)
	{
		for (const auto unit : BWAPI::Broodwar->self()->getUnits())
		{
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord)
			{
				ScoutManager::Instance().setOverlordScout(unit);
				assignUnit(unit, _scoutUnits);
				break;
			}
		}
	}

    // Send a scout worker if we haven't yet and should.
	if (!_initialScoutTime)
    {
		if (ScoutManager::Instance().shouldScout())
		{
			BWAPI::Unit workerScout = getScoutWorker();

			// If we find a worker, make it the scout unit.
			if (workerScout)
			{
				ScoutManager::Instance().setWorkerScout(workerScout);
				assignUnit(workerScout, _scoutUnits);
                _initialScoutTime = BWAPI::Broodwar->getFrameCount();
			}
		}
    }
}

// Set combat units to be passed to CombatCommander.
void GameCommander::setCombatUnits()
{
	for (const auto unit : _validUnits)
	{
		if (!isAssigned(unit) && (UnitUtil::IsCombatUnit(unit) || unit->getType().isWorker()))		
		{	
			assignUnit(unit, _combatUnits);
		}
	}
}

void GameCommander::surrender()
{
	_surrenderTime = BWAPI::Broodwar->getFrameCount();
}

void GameCommander::onEnd(bool isWinner)
{
    OpponentModel::Instance().setWin(isWinner);
    OpponentModel::Instance().write();
}

void GameCommander::onUnitShow(BWAPI::Unit unit)			
{ 
	InformationManager::Instance().onUnitShow(unit); 
	WorkerManager::Instance().onUnitShow(unit);
}

void GameCommander::onUnitHide(BWAPI::Unit unit)			
{ 
	InformationManager::Instance().onUnitHide(unit); 
}

void GameCommander::onUnitCreate(BWAPI::Unit unit)		
{ 
	InformationManager::Instance().onUnitCreate(unit); 
}

void GameCommander::onUnitComplete(BWAPI::Unit unit)
{
	InformationManager::Instance().onUnitComplete(unit);
}

void GameCommander::onUnitRenegade(BWAPI::Unit unit)		
{ 
	InformationManager::Instance().onUnitRenegade(unit); 
}

void GameCommander::onUnitDestroy(BWAPI::Unit unit)		
{ 	
	ProductionManager::Instance().onUnitDestroy(unit);
	WorkerManager::Instance().onUnitDestroy(unit);
	InformationManager::Instance().onUnitDestroy(unit); 
	StrategyManager::Instance().onUnitDestroy(unit); 
}

void GameCommander::onUnitMorph(BWAPI::Unit unit)		
{ 
	InformationManager::Instance().onUnitMorph(unit);
	WorkerManager::Instance().onUnitMorph(unit);
}

BWAPI::Unit GameCommander::getScoutWorker()
{
    // If there is a proxy builder, use it
    // We set up the build timings such that it will have time to scout before being required for more proxy buildings
    if (WorkerManager::Instance().getProxyBuilder())
        return WorkerManager::Instance().getProxyBuilder();

	// We get the free worker closest to the center of the map by ground
    BWAPI::Position mapCenter(BWAPI::TilePosition(BWAPI::Broodwar->mapWidth() / 2, BWAPI::Broodwar->mapHeight() / 2));
	BWAPI::Unit bestUnit = nullptr;
    int bestDist = INT_MAX;

	for (const auto unit : _validUnits)
	{
		if (unit->getType().isWorker() &&
			!isAssigned(unit) &&
			WorkerManager::Instance().isFree(unit) &&
			!unit->isCarryingMinerals() &&
			!unit->isCarryingGas() &&
			unit->getOrder() != BWAPI::Orders::MiningMinerals)
		{
            int dist = PathFinding::GetGroundDistance(unit->getPosition(), mapCenter, unit->getType());
			if (dist < bestDist)
			{
				bestDist = dist;
				bestUnit = unit;
			}
		}
	}

	return bestUnit;
}

void GameCommander::assignUnit(BWAPI::Unit unit, BWAPI::Unitset & set)
{
    if (_scoutUnits.contains(unit)) { _scoutUnits.erase(unit); }
    else if (_combatUnits.contains(unit)) { _combatUnits.erase(unit); }

    set.insert(unit);
}

// Decide whether to give up early. See config option SurrenderWhenHopeIsLost.
bool GameCommander::surrenderMonkey()
{
	if (!Config::Strategy::SurrenderWhenHopeIsLost)
	{
		return false;
	}

	// Only check once every five seconds. No hurry to give up.
	if (BWAPI::Broodwar->getFrameCount() % (5 * 24) != 0)
	{
		return false;
	}

	// Surrender if all conditions are met:
	// 1. We don't have the cash to make a worker.
	// 2. We have no unit that can attack.
	// 3. The enemy has at least one visible unit that can destroy buildings.
	// Terran does not float buildings, so we check whether the enemy can attack ground.

	// 1. Our cash.
	if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Nexus) > 0 && BWAPI::Broodwar->self()->minerals() >= 50)
	{
		return false;
	}

	// 2. Our units.
	for (const auto unit : _validUnits)
	{
		if (unit->canAttack())
		{
			return false;
		}
	}

	// 3. Enemy units.
	bool safe = true;
	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->isVisible() && UnitUtil::CanAttackGround(unit))
		{
			safe = false;
			break;
		}
	}
	if (safe)
	{
		return false;
	}

	// Surrender monkey says surrender!
	return true;
}

GameCommander & GameCommander::Instance()
{
	static GameCommander instance;
	return instance;
}
