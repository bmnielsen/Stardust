#pragma once

#include "Common.h"

#include <bwem.h>

class MyUnit
{
public:
    MyUnit(BWAPI::Unit unit);

    void update();

    bool moveTo(BWAPI::Position position, bool avoidNarrowChokes = false);
    void fleeFrom(BWAPI::Position position);
    int  distanceToMoveTarget() const;

    bool isReady() const;
    bool isStuck() const;

    int getLastAttackStartedAt() const { return lastAttackStartedAt; };

    void move(BWAPI::Position position);
    void rightClick(BWAPI::Unit target);
    bool build(BWAPI::UnitType type, BWAPI::TilePosition tile);

private:
    BWAPI::Unit     unit;

    bool issuedOrderThisFrame;

    // Used for pathing
    BWAPI::Position                     targetPosition;
    BWAPI::Position                     currentlyMovingTowards;
    std::deque<const BWEM::ChokePoint*> waypoints;
    BWAPI::Unit                         mineralWalkingPatch;
    const BWEM::Area*                   mineralWalkingTargetArea;
    BWAPI::Position                     mineralWalkingStartPosition;
    int                                 lastMoveFrame;

    // Used for various things, like detecting stuck goons and updating our collision matrix
    BWAPI::Position lastPosition;

    // Used for goon micro
    int lastAttackStartedAt;
    int potentiallyStuckSince;  // frame the unit might have been stuck since, or 0 if it isn't stuck

    void updateMoveWaypoints();
    void moveToNextWaypoint();
    void mineralWalk();

    void updateGoon();
};
