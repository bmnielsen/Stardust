#pragma once;

#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroQueens : public MicroManager
{
	bool aboutToDie(const BWAPI::Unit queen) const;

	int parasiteScore(BWAPI::Unit u) const;
	bool maybeParasite(BWAPI::Unit queen);

    int ensnareScore(BWAPI::Unit u) const;
    bool maybeEnsnare(BWAPI::Unit queen);

    int broodlingScore(BWAPI::Unit queen, BWAPI::Unit u) const;
    bool maybeBroodling(BWAPI::Unit queen);

    BWAPI::Position getQueenDestination(BWAPI::Unit queen, const BWAPI::Position & target) const;

    // The different updates are done on different frames to spread out the work.
	void updateMovement(BWAPI::Unit vanguard);
    void updateAction();

public:
	MicroQueens();
	void executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster);

	void update(BWAPI::Unit vanguard);
};
}
