#pragma once

#include "Common.h"
#include "GameRecord.h"

// The game record of the current game, updated as we go along.

namespace UAlbertaBot
{
// Enough tech that we can tell which units are possible.
enum class GameTech : int
    { Barracks
    , Academy
    , Factory
    , MachineShop
    , Armory
    , Starport
    , ControlTower
    , ScienceFac
    , PhysicsLab
    , CovertOps
    , Nuke

    , Zealot
    , Dragoon
    , Templar
    , Robo
    , Reaver
    , Observer
    , Stargate
    , Carrier
    , Arbiter

    , Zergling
    , Hydralisk
    , Lurker
    , Lair
    , Hive
    , Spire
    , GreaterSpire
    , Queen
    , Infested
    , Ultralisk
    , Defiler

    , Size
    };

class GameRecordNow : public GameRecord
{
private:
    std::map<GameTech, int> techHistory;

	void takeSnapshot();

    bool enemyScoutedUs() const;

public:
	GameRecordNow();

	void setOpening(const std::string & opening) { openingName = opening; };
	void setWin(bool isWinner);

	void update();
};

}