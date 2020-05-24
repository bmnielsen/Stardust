#include "Unit.h"

#include "Geo.h"
#include "Players.h"
#include "Map.h"

namespace
{
    bool isUndetected(BWAPI::Unit unit)
    {
        return (unit->isBurrowed() || unit->isCloaked() || unit->getType().hasPermanentCloak()) && !unit->isDetected();
    }

    int estimateCompletionFrame(BWAPI::Unit unit)
    {
        if (!unit->getType().isBuilding() || unit->isCompleted()) return -1;

        int remainingHitPoints = unit->getType().maxHitPoints() - unit->getHitPoints();
        double hitPointsPerFrame = (unit->getType().maxHitPoints() * 0.9) / unit->getType().buildTime();
        return BWAPI::Broodwar->getFrameCount() + (int) (remainingHitPoints / hitPointsPerFrame);
    }

    BWAPI::TilePosition computeBuildTile(BWAPI::Unit unit)
    {
        if (unit->getType().isBuilding() && !unit->isFlying())
        {
            return unit->getTilePosition();
        }

        return BWAPI::TilePositions::Invalid;
    }
}

UnitImpl::UnitImpl(BWAPI::Unit unit)
        : bwapiUnit(unit)
        , player(unit->getPlayer())
        , tilePositionX(unit->getPosition().x >> 5U)
        , tilePositionY(unit->getPosition().y >> 5U)
        , buildTile(computeBuildTile(unit))
        , lastSeen(BWAPI::Broodwar->getFrameCount())
        , lastSeenAttacking(-1)
        , type(unit->getType())
        , id(unit->getID())
        , lastPosition(unit->getPosition())
        , lastPositionValid(true)
        , lastPositionVisible(true)
        , beingManufacturedOrCarried(false)
        , lastHealth(unit->getHitPoints())
        , lastShields(unit->getShields())
        , completed(unit->isCompleted())
        , estimatedCompletionFrame(-1)
        , isFlying(unit->isFlying())
        , isMoving(std::abs(unit->getVelocityX()) > 0.001 || std::abs(unit->getVelocityY()) > 0.001)
        , cooldownUntil(BWAPI::Broodwar->getFrameCount() + std::max(unit->getGroundWeaponCooldown(), unit->getAirWeaponCooldown()))
        , stimmedUntil(BWAPI::Broodwar->getFrameCount() + unit->getStimTimer())
        , undetected(isUndetected(unit))
        , burrowed(unit->isBurrowed())
        , lastBurrowing(unit->getOrder() == BWAPI::Orders::Burrowing ? BWAPI::Broodwar->getFrameCount() : 0)
        , doomed(false) {}

void UnitImpl::created()
{
    beingManufacturedOrCarried = isBeingManufacturedOrCarried();

    update(bwapiUnit);

    if (!beingManufacturedOrCarried)
    {
        Players::grid(player).unitCreated(type, lastPosition, completed, burrowed);
#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitCreated " << lastPosition;
        Log::Debug() << *this << ": Grid::unitCreated " << lastPosition;
#endif

        CherryVis::unitFirstSeen(bwapiUnit);
    }
}

void UnitImpl::update(BWAPI::Unit unit)
{
    if (!unit || !unit->exists()) return;

    updateGrid(unit);

    player = unit->getPlayer();
    type = unit->getType();

    lastSeen = BWAPI::Broodwar->getFrameCount();
    if (unit->isAttacking()) lastSeenAttacking = BWAPI::Broodwar->getFrameCount();

    tilePositionX = unit->getPosition().x >> 5U;
    tilePositionY = unit->getPosition().y >> 5U;
    lastPosition = unit->getPosition();
    lastPositionValid = true;
    lastPositionVisible = true;
    beingManufacturedOrCarried = isBeingManufacturedOrCarried();

    // Cloaked units show up with 0 hit points and shields, so default to max and otherwise don't touch them
    undetected = isUndetected(unit);
    if (!undetected)
    {
        lastHealth = unit->getHitPoints();
        lastShields = unit->getShields();
    }
    else if (lastHealth == 0)
    {
        lastHealth = unit->getType().maxHitPoints();
        lastShields = unit->getType().maxShields();
    }

    burrowed = unit->isBurrowed();
    lastBurrowing = unit->getOrder() == BWAPI::Orders::Burrowing ? BWAPI::Broodwar->getFrameCount() : 0;

    completed = unit->isCompleted();
    estimatedCompletionFrame = estimateCompletionFrame(unit);

    // TODO: Track lifted buildings
    isFlying = unit->isFlying();
    isMoving = std::abs(unit->getVelocityX()) > 0.001 || std::abs(unit->getVelocityY()) > 0.001;

    if (unit->isVisible())
    {
        cooldownUntil = BWAPI::Broodwar->getFrameCount() + std::max(unit->getGroundWeaponCooldown(), unit->getAirWeaponCooldown());
        stimmedUntil = BWAPI::Broodwar->getFrameCount() + unit->getStimTimer();
    }

    // Process upcoming attacks
    int upcomingDamage = 0;
    for (auto it = upcomingAttacks.begin(); it != upcomingAttacks.end();)
    {
        // Remove bullets when they have hit
        if (it->bullet && (!it->bullet->exists() || it->bullet->getID() != it->bulletId
                           || it->bullet->getPosition().getApproxDistance(it->bullet->getTargetPosition()) == 0))
        {
            it = upcomingAttacks.erase(it);
            continue;
        }

            // Clear if the attacker is dead, no longer visible, or out of range
        else if (!it->attacker || !it->attacker->exists() || !it->attacker->bwapiUnit->isVisible() || !isInEnemyWeaponRange(it->attacker))
        {
            it = upcomingAttacks.erase(it);
            continue;
        }

            // Clear when the attacker has finished making an attack
        else if ((isFlying ? it->attacker->bwapiUnit->getAirWeaponCooldown() : it->attacker->bwapiUnit->getGroundWeaponCooldown())
                 > BWAPI::Broodwar->getRemainingLatencyFrames()
                 && !it->attacker->bwapiUnit->isAttackFrame())
        {
            it = upcomingAttacks.erase(it);
            continue;
        }

        upcomingDamage += it->damage;
        it++;
    }

    doomed = (upcomingDamage >= (lastHealth + lastShields));
    if (doomed) CherryVis::log(id) << "DOOMED!";
}

void UnitImpl::updateUnitInFog()
{
    // If we've already detected that this unit has moved from its last known location, skip it
    if (!lastPositionValid) return;

    bool positionVisible = BWAPI::Broodwar->isVisible(tilePositionX, tilePositionY);

    // Detect burrowed units we have observed burrowing
    if (!burrowed && positionVisible && lastBurrowing == BWAPI::Broodwar->getFrameCount() - 1)
    {
        // Update grid
        Players::grid(player).unitMoved(type, lastPosition,  true, type, lastPosition, false);
#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitMoved (observed burrowing)";
        Log::Debug() << *this << ": Grid::unitMoved (observed burrowing)";
#endif

        burrowed = true;
    }

    // If the last position has been visible for two consecutive frames, the unit is gone
    if (positionVisible && lastPositionVisible)
    {
        // Units that we know have been burrowed at this position might still be there
        if (burrowed)
        {
            // Assume the unit is still burrowed here unless we have detection on the position or the unit was doomed before it burrowed
            auto grid = Players::grid(BWAPI::Broodwar->self());
            if (!doomed && grid.detection(lastPosition) == 0) return;
        }

        lastPositionValid = false;

        // TODO: Track lifted buildings
    }

        // If the unit is a sieged tank, assume it is gone from its last position
        // if we haven't seen it in 10 seconds and have a unit that it would otherwise
        // fire upon
    else if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode &&
             lastSeen < (BWAPI::Broodwar->getFrameCount() - 240))
    {
        // Look for a unit inside the tank's sight range
        // TODO: Could also look for a unit inside the tank's weapon range that the enemy can see
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (Geo::EdgeToEdgeDistance(
                    unit->getType(),
                    unit->getPosition(),
                    BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode,
                    lastPosition) <= BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.sightRange())
            {
                lastPositionValid = false;
                break;
            }
        }
    }

    // For the grid, treat units with invalid last position as destroyed
    if (!lastPositionValid)
    {
        Players::grid(player).unitDestroyed(type, lastPosition, completed, burrowed);
#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitDestroyed (!LPV) " << lastPosition;
        Log::Debug() << *this << ": Grid::unitDestroyed (!LPV) " << lastPosition;
#endif
    }

    // Mark buildings complete in the grid if we expect them to have completed in the fog
    if (lastPositionValid && !completed && estimatedCompletionFrame != -1 && estimatedCompletionFrame <= BWAPI::Broodwar->getFrameCount())
    {
        completed = true;
        estimatedCompletionFrame = -1;
        Players::grid(player).unitCompleted(type, lastPosition, burrowed);
#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitCompleted (FOG) " << lastPosition;
        Log::Debug() << *this << ": Grid::unitCompleted (FOG) " << lastPosition;
#endif
    }

    lastPositionVisible = positionVisible;
}

void UnitImpl::addUpcomingAttack(const Unit &attacker, BWAPI::Bullet bullet)
{
    // Remove any existing upcoming attack from this attacker
    for (auto it = upcomingAttacks.begin(); it != upcomingAttacks.end();)
    {
        if (it->attacker == attacker)
            it = upcomingAttacks.erase(it);
        else
            it++;
    }

    upcomingAttacks.emplace_back(attacker,
                                 bullet,
                                 Players::attackDamage(bullet->getSource()->getPlayer(), bullet->getSource()->getType(), player, type));
}

void UnitImpl::updateGrid(BWAPI::Unit unit)
{
    auto &grid = Players::grid(unit->getPlayer());

    // Units that have renegaded
    if (unit->getPlayer() != player)
    {
        Players::grid(player).unitDestroyed(type, lastPosition, completed, burrowed);
        grid.unitCreated(unit->getType(), unit->getPosition(), unit->isCompleted(), unit->isBurrowed());
#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitDestroyed (renegade) " << lastPosition;
        CherryVis::log(id) << "Grid::unitCreated (renegade) " << unit->getPosition();
        Log::Debug() << *this << ": Grid::unitDestroyed (renegade) " << lastPosition;
        Log::Debug() << *this << ": Grid::unitCreated (renegade) " << unit->getPosition();
#endif
        return;
    }

    // Units that have morphed
    if (type != unit->getType())
    {
        grid.unitDestroyed(type, lastPosition, completed, burrowed);
        grid.unitCreated(unit->getType(), unit->getPosition(), unit->isCompleted(), unit->isBurrowed());
#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitDestroyed (morph) " << lastPosition;
        CherryVis::log(id) << "Grid::unitCreated (morph) " << unit->getPosition();
        Log::Debug() << *this << ": Grid::unitDestroyed (morph) " << lastPosition;
        Log::Debug() << *this << ": Grid::unitCreated (morph) " << unit->getPosition();
#endif
        return;
    }

    // Units that have changed manufactured or carried
    if (beingManufacturedOrCarried != isBeingManufacturedOrCarried())
    {
        // If no longer being manufactured or carried, treat as a new unit
        if (beingManufacturedOrCarried)
        {
            grid.unitCreated(unit->getType(), unit->getPosition(), unit->isCompleted(), unit->isBurrowed());
#if DEBUG_GRID_UPDATES
            CherryVis::log(id) << "Grid::unitCreated (manufactured) " << unit->getPosition();
            Log::Debug() << *this << ": Grid::unitCreated (manufactured) " << unit->getPosition();
#endif

            // If this is the incomplete to complete transition, register in CherryVis
            if (!completed && unit->isCompleted())
            {
                CherryVis::unitFirstSeen(bwapiUnit);
            }

            return;
        }

        // Otherwise treat it as destroyed until it shows up again
        grid.unitDestroyed(type, lastPosition, completed, burrowed);
#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitDestroyed (carried) " << lastPosition;
        Log::Debug() << *this << ": Grid::unitDestroyed (carried) " << lastPosition;
#endif

        return;
    }

    // Units that have completed
    if (!completed && unit->isCompleted())
    {
        grid.unitCompleted(type, lastPosition, burrowed);
#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitCompleted " << unit->getPosition();
        Log::Debug() << *this << ": Grid::unitCompleted " << unit->getPosition();
#endif

        // Fall through as the unit may have moved
    }

    // Units that have "incompleted"
    // This either means we assumed something completed in the fog that didn't, or a morph is happening (e.g. hydralist to lurker egg)
    // In either case we just recreate the unit in the grid
    if (completed && !unit->isCompleted())
    {
        grid.unitDestroyed(type, lastPosition, true, burrowed);
        grid.unitCreated(unit->getType(), unit->getPosition(), false, unit->isBurrowed());
#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitDestroyed (incomplete) " << lastPosition;
        CherryVis::log(id) << "Grid::unitCreated (incomplete) " << unit->getPosition();
        Log::Debug() << *this << ": Grid::unitDestroyed (incomplete) " << type << "@" << lastPosition;
        Log::Debug() << *this << ": Grid::unitCreated (incomplete) " << unit->getType() << "@" << unit->getPosition();
#endif
        return;
    }

    // Units that moved while in the fog and have now reappeared
    if (!lastPositionValid)
    {
        grid.unitCreated(unit->getType(), unit->getPosition(), unit->isCompleted(), unit->isBurrowed());
#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitCreated (LPV) " << unit->getPosition();
        Log::Debug() << *this << ": Grid::unitCreated (LPV) " << unit->getPosition();
#endif
        return;
    }

    // Units that have taken off
    // We can just treat them as destroyed, as nothing that can take off has an attack
    if (!isFlying && unit->isFlying())
    {
        grid.unitDestroyed(type, lastPosition, completed, burrowed);

        // Also affects navigation grids, so tell the map when a building has lifted
        if (type.isBuilding())
        {
            auto topLeft = lastPosition - (BWAPI::Position(type.tileSize()) / 2);
            Map::onBuildingLifted(type, BWAPI::TilePosition(topLeft));
        }

#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitDestroyed (liftoff) " << lastPosition;
        Log::Debug() << *this << ": Grid::unitDestroyed (liftoff) " << lastPosition;
#endif
        return;
    }

    // Units that have landed
    if (isFlying && !unit->isFlying())
    {
        grid.unitCreated(unit->getType(), unit->getPosition(), unit->isCompleted(), unit->isBurrowed());

        // Also affects navigation grids, so tell the map the unit has landed
        if (type.isBuilding())
        {
            Map::onBuildingLanded(type, unit->getTilePosition());
        }

#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitCreated (landed) " << unit->getPosition();
        Log::Debug() << *this << ": Grid::unitCreated (landed) " << unit->getPosition();
#endif
        return;
    }

    // Units that have moved or changed burrow state
    if (lastPosition != unit->getPosition() || burrowed != unit->isBurrowed())
    {
        grid.unitMoved(unit->getType(), unit->getPosition(), unit->isBurrowed(), type, lastPosition, burrowed);
#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitMoved " << lastPosition << " to " << unit->getPosition()
                           << "; burrow " << burrowed << " to " << unit->isBurrowed();
        Log::Debug() << *this << ": Grid::unitMoved " << lastPosition << " to " << unit->getPosition()
                              << "; burrow " << burrowed << " to " << unit->isBurrowed();
#endif
    }

    // TODO: Workers in a refinery
}
