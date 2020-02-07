#include "GameRecordNow.h"

#include "Bases.h"
#include "GameCommander.h"
#include "InformationManager.h"

using namespace UAlbertaBot;

// Take a digest snapshot of the game situation.
void GameRecordNow::takeSnapshot()
{
	snapshots.push_back(new GameSnapshot(PlayerSnapshot (BWAPI::Broodwar->self()), PlayerSnapshot (BWAPI::Broodwar->enemy())));
}

// Figure out whether the enemy has seen our base yet.
bool GameRecordNow::enemyScoutedUs() const
{
	Base * base = Bases::Instance().myStartingBase();

	for (const auto & kv : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
	{
		const UnitInfo & ui(kv.second);

		// If a unit was last spotted close to us, assume we've been seen.
		if (ui.lastPosition.getDistance(base->getCenter()) < 800)
		{
			return true;
		}
	}

	return false;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

// Constructor for the record of the current game.
// When this object is initialized, the opening and some other items are not yet known.
GameRecordNow::GameRecordNow()
{
}

// Called when the game is over.
void GameRecordNow::setWin(bool isWinner)
{
	win = isWinner;
	frameGameEnds = BWAPI::Broodwar->getFrameCount();
}

void GameRecordNow::update()
{
	int now = BWAPI::Broodwar->getFrameCount();

	// Update the when-it-happens frame counters. We don't actually need to check often.
	if (now % 32 == 30)
	{
		if (enemyRace == BWAPI::Races::Unknown)
		{
			enemyRace = BWAPI::Broodwar->enemy()->getRace();
		}
		enemyPlan = OpponentModel::Instance().getEnemyPlan();
		if (!frameEnemyScoutsOurBase)
		{
			if (enemyScoutedUs())
			{
				frameEnemyScoutsOurBase = now;
			}
		}
		if (!frameScoutSentForGasSteal && GameCommander::Instance().getScoutTime() && ScoutManager::Instance().tryGasSteal())
		{
			// Not now, but the time when the scout was first sent out.
			frameScoutSentForGasSteal = GameCommander::Instance().getScoutTime();
		}
		if (ScoutManager::Instance().gasStealQueued())
		{
			// We at least got close enough to queue up the building. Let's pretend that means it started.
			gasStealHappened = true;
		}
		if (!frameEnemyGetsCombatUnits && InformationManager::Instance().enemyHasCombatUnits())
		{
			frameEnemyGetsCombatUnits = now;
		}
		if (!frameEnemyGetsAirUnits && InformationManager::Instance().enemyHasAirTech())
		{
			frameEnemyGetsAirUnits = now;
		}
		if (!frameEnemyGetsStaticAntiAir && InformationManager::Instance().enemyHasStaticAntiAir())
		{
			frameEnemyGetsStaticAntiAir = now;
		}
		if (!frameEnemyGetsMobileAntiAir && InformationManager::Instance().enemyHasAntiAir())
		{
			frameEnemyGetsMobileAntiAir = now;
		}
		if (!frameEnemyGetsCloakedUnits && InformationManager::Instance().enemyHasCloakTech())
		{
			frameEnemyGetsCloakedUnits = now;
		}
		if (!frameEnemyGetsStaticDetection && InformationManager::Instance().enemyHasStaticDetection())
		{
			frameEnemyGetsStaticDetection = now;
		}
		if (!frameEnemyGetsMobileDetection && InformationManager::Instance().enemyHasMobileDetection())
		{
			frameEnemyGetsMobileDetection = now;
		}
	}

	// If it's time, take a snapshot.
	int sinceFirst = now - firstSnapshotTime;
	if (sinceFirst >= 0 && sinceFirst % snapshotInterval == 0)
	{
		takeSnapshot();
	}
}
