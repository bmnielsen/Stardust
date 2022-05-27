#include "Unit.h"

#include "Geo.h"
#include "Players.h"
#include "Map.h"
#include "General.h"
#include "UnitUtil.h"
#include <iomanip>

#include "DebugFlag_GridUpdates.h"

#if INSTRUMENTATION_ENABLED
#define UPCOMING_ATTACKS_DEBUG false
#define PREDICTED_POSITIONS_DEBUG true
#endif

namespace
{
    bool isUndetected(BWAPI::Unit unit)
    {
        return (unit->isBurrowed() || unit->isCloaked() || unit->getType().hasPermanentCloak()) && !unit->isDetected();
    }

    int estimateCompletionFrame(BWAPI::Unit unit)
    {
        if (!unit->getType().isBuilding() || unit->isCompleted()) return -1;

        if (unit->getPlayer() == BWAPI::Broodwar->self()) return unit->getRemainingBuildTime();

        int remainingHitPoints = unit->getType().maxHitPoints() - unit->getHitPoints();
        double hitPointsPerFrame = (unit->getType().maxHitPoints() * 0.9) / unit->getType().buildTime();
        return currentFrame + (int) (remainingHitPoints / hitPointsPerFrame);
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
        , distToTargetPosition(-1)
        , lastCommandFrame(-1)
        , lastSeen(currentFrame)
        , lastSeenAttacking(-1)
        , type(unit->getType())
        , id(unit->getID())
        , lastPosition(unit->getPosition())
        , lastPositionValid(true)
        , lastPositionVisible(true)
        , lastAngle(unit->getAngle())
        , beingManufacturedOrCarried(false)
        , frameLastMoved(currentFrame)
        , simPosition(unit->getPosition())
        , simPositionValid(true)
        , predictedPositionsUpdated(false)
        , bwHeading(0)
        , bwHeadingUpdated(false)
        , bwSpeed(0)
        , bwSpeedUpdated(false)
        , lastHealth(unit->getHitPoints())
        , lastShields(unit->getShields())
        , health(unit->getHitPoints())
        , shields(unit->getShields())
        , lastHealFrame(-1)
        , lastAttackedFrame(-1)
        , completed(unit->isCompleted())
        , estimatedCompletionFrame(-1)
        , isFlying(unit->isFlying())
        , cooldownUntil(currentFrame + std::max(unit->getGroundWeaponCooldown(), unit->getAirWeaponCooldown()))
        , stimmedUntil(currentFrame + unit->getStimTimer())
        , undetected(isUndetected(unit))
        , immobile(unit->isStasised() || unit->isLockedDown())
        , burrowed(unit->isBurrowed())
        , lastBurrowing(unit->getOrder() == BWAPI::Orders::Burrowing ? currentFrame : 0)
        , orderProcessTimer(-1)
        , lastTarget(nullptr) {}

void UnitImpl::created()
{
    predictedPositions.resize(BWAPI::Broodwar->getLatencyFrames() + 2);

    beingManufacturedOrCarried = isBeingManufacturedOrCarried();

    update(bwapiUnit);

    if (!beingManufacturedOrCarried)
    {
        Players::grid(player).unitCreated(type, lastPosition, completed, burrowed, immobile);
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

    distToTargetPosition = -1;

    // Convert last command frame to our frame counter
    if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() - 1)
    {
        lastCommandFrame = currentFrame - (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame());
    }

    updateGrid(unit);

    player = unit->getPlayer();
    type = unit->getType();

    lastSeen = currentFrame;

    if (lastPosition != unit->getPosition())
    {
        frameLastMoved = currentFrame;
    }

    tilePositionX = unit->getPosition().x >> 5U;
    tilePositionY = unit->getPosition().y >> 5U;
    lastPosition = simPosition = unit->getPosition();
    lastAngle = unit->getAngle();
    offsetToVanguardUnit = {};
    lastPositionValid = simPositionValid = true;
    lastPositionVisible = true;
    beingManufacturedOrCarried = isBeingManufacturedOrCarried();

    std::fill(predictedPositions.begin(), predictedPositions.end(), lastPosition);
    predictedPositionsUpdated = false;

    bwHeadingUpdated = false;
    bwSpeedUpdated = false;

    if (player->getRace() == BWAPI::Races::Terran && unit->isCompleted() && unit->getHitPoints() > lastHealth)
    {
        lastHealFrame = currentFrame;
    }

    // Currently ignoring Terran buildings since they can burn, should track this better if we want to use this for them
    if (unit->getShields() < lastShields || (unit->getHitPoints() < lastHealth && (player->getRace() != BWAPI::Races::Terran || !type.isBuilding())))
    {
        lastAttackedFrame = currentFrame;
    }

    // Cloaked units show up with 0 hit points and shields, so default to max and otherwise don't touch them
    undetected = isUndetected(unit);
    if (!undetected)
    {
        lastHealth = unit->getHitPoints();
        lastShields = unit->getShields();
        health = unit->getHitPoints();
        shields = unit->getShields();
    }
    else if (lastHealth == 0)
    {
        lastHealth = unit->getType().maxHitPoints();
        lastShields = unit->getType().maxShields();
        health = unit->getType().maxHitPoints();
        shields = unit->getType().maxShields();
    }

    immobile = unit->isStasised() || unit->isLockedDown();

    burrowed = unit->isBurrowed();
    lastBurrowing = unit->getOrder() == BWAPI::Orders::Burrowing ? currentFrame : 0;

    completed = unit->isCompleted();
    estimatedCompletionFrame = estimateCompletionFrame(unit);

    // TODO: Track lifted buildings
    isFlying = unit->isFlying();

    if (unit->isVisible())
    {
        auto cooldown = std::max(unit->getGroundWeaponCooldown(), unit->getAirWeaponCooldown());
        if (cooldown > 0)
        {
            if (cooldown > (cooldownUntil - currentFrame + 1))
            {
                lastSeenAttacking = currentFrame;
                orderProcessTimer = 0;
            }
            else if (cooldown > 0 && orderProcessTimer == 0 && lastTarget && lastTarget->exists() && lastTarget->lastPositionValid
                && getDistance(lastTarget) < range(lastTarget))
            {
                orderProcessTimer = cooldown;
            }
        }
        cooldownUntil = currentFrame + cooldown;
        stimmedUntil = currentFrame + unit->getStimTimer();
    }

    // Process upcoming attacks
    int upcomingDamage = 0;
    for (auto it = upcomingAttacks.begin(); it != upcomingAttacks.end();)
    {
        // Remove attacks when they expire
        if (currentFrame >= it->expiryFrame)
        {
#if UPCOMING_ATTACKS_DEBUG
            CherryVis::log(id) << "Clearing attack from " << *(it->attacker) << " as it has expired";
#endif
            it = upcomingAttacks.erase(it);
            continue;
        }

        // Remove bullets when they have hit
        if (it->bullet)
        {
            if (!it->bullet->exists() || it->bullet->getID() != it->bulletId
                || it->bullet->getPosition().getApproxDistance(it->bullet->getTargetPosition()) == 0)
            {
#if UPCOMING_ATTACKS_DEBUG
                CherryVis::log(id) << "Clearing attack from " << *(it->attacker) << " as bullet has hit";
#endif
                it = upcomingAttacks.erase(it);
                continue;
            }
        }

            // Clear if the attacker is dead, no longer visible, or out of range
        else if (!it->attacker || !it->attacker->exists() || !it->attacker->bwapiUnit->isVisible() || !isInEnemyWeaponRange(it->attacker))
        {
#if UPCOMING_ATTACKS_DEBUG
            CherryVis::log(id) << "Clearing attack from " << *(it->attacker) << " as the attacker is gone";
#endif
            it = upcomingAttacks.erase(it);
            continue;
        }

        upcomingDamage += it->damage;
        it++;
    }

#if UPCOMING_ATTACKS_DEBUG
    if (upcomingDamage > 0)
    {
        CherryVis::log(id) << "Total value of upcoming attacks is " << upcomingDamage
                           << "; current health is " << health << " (" << shields << ")";
    }
#endif

    // Do not simulate damage if the unit is being healed
    if (!isBeingHealed())
    {
        if (upcomingDamage > 0 && shields > 0)
        {
            int shieldDamage = std::min(shields, upcomingDamage);
            upcomingDamage -= shieldDamage;
            shields -= shieldDamage;
        }
        if (upcomingDamage > 0)
        {
            health = std::max(0, health - upcomingDamage);
            if (health <= 0) CherryVis::log(id) << "DOOMED!";
        }
    }
}

void UnitImpl::updateUnitInFog()
{
    bool positionVisible = BWAPI::Broodwar->isVisible(tilePositionX, tilePositionY);

    // Detect burrowed units we have observed burrowing
    if (lastBurrowing == currentFrame - 1 && !burrowed && positionVisible)
    {
        // Update grid
        Players::grid(player).unitMoved(type, lastPosition, true, immobile, type, lastPosition, false, immobile);
#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitMoved (observed burrowing)";
    Log::Debug() << *this << ": Grid::unitMoved (observed burrowing)";
#endif

        burrowed = true;
    }

    // For predicted positions, units in the fog stay put
    // So just reset the vector on the first frame it is in the fog
    if (lastSeen == currentFrame - 1)
    {
        std::fill(predictedPositions.begin(), predictedPositions.end(), lastPosition);
        predictedPositionsUpdated = true;
    }

    // Update position simulation
    auto simPositionSucceeded = updateSimPosition();

    // If we've already detected that this unit has moved from its last known location, skip it
    if (!lastPositionValid) return;

    // If the unit has a valid simulated position, treat its last position as invalid
    if (simPositionSucceeded)
    {
        lastPositionValid = false;
    }

        // If the last position has been visible for two consecutive frames, the unit is gone
    else if (positionVisible && lastPositionVisible && lastSeen < (currentFrame - 1))
    {
        // Units that we know have been burrowed at this position might still be there
        if (burrowed)
        {
            // Assume the unit is still burrowed here unless we have detection on the position or the unit was doomed before it burrowed
            auto &grid = Players::grid(BWAPI::Broodwar->self());
            if (health > 0 && grid.detection(lastPosition) == 0) return;
        }

        lastPositionValid = false;

        // TODO: Track lifted buildings
    }

        // If the unit is a sieged tank, assume it is gone from its last position
        // if we haven't seen it in 10 seconds and have a unit that it would otherwise
        // fire upon
    else if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode &&
             lastSeen < (currentFrame - 240))
    {
        // Look for a unit inside the tank's sight range
        // TODO: Could also look for a unit inside the tank's weapon range that the enemy can see
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (!unit->getType().isBuilding() && !unit->isCompleted()) continue;
            if (unit->isLoaded()) continue;
            if (unit->isFlying()) continue;
            if (unit->isStasised()) continue;

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
        // Ignore flying buildings that have moved in the fog
        if (!type.isBuilding() || !isFlying)
        {
            Players::grid(player).unitDestroyed(type, lastPosition, completed, burrowed, immobile);
#if DEBUG_GRID_UPDATES
            CherryVis::log(id) << "Grid::unitDestroyed (!LPV) " << lastPosition;
            Log::Debug() << *this << ": Grid::unitDestroyed (!LPV) " << lastPosition;
#endif
        }
    }

    // Mark buildings complete in the grid if we expect them to have completed in the fog
    if (lastPositionValid && !completed && estimatedCompletionFrame != -1 && estimatedCompletionFrame <= currentFrame)
    {
        completed = true;
        estimatedCompletionFrame = -1;
        Players::grid(player).unitCompleted(type, lastPosition, burrowed, immobile);
#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitCompleted (FOG) " << lastPosition;
        Log::Debug() << *this << ": Grid::unitCompleted (FOG) " << lastPosition;
#endif
    }

    lastPositionVisible = positionVisible;
}

void UnitImpl::addUpcomingAttack(const Unit &attacker, BWAPI::Bullet bullet)
{
    // Bullets always remove any existing upcoming attack from this attacker
    for (auto it = upcomingAttacks.begin(); it != upcomingAttacks.end();)
    {
        if (it->attacker == attacker)
        {
#if UPCOMING_ATTACKS_DEBUG
            CherryVis::log(id) << "Removing upcoming attack from " << *attacker << " because of created bullet";
#endif
            it = upcomingAttacks.erase(it);
        }
        else
            it++;
    }

#if UPCOMING_ATTACKS_DEBUG
    CherryVis::log(id) << "Adding upcoming attack (bullet) from " << *attacker;
#endif

    // If the attack is from low-ground to high-ground, don't add the attack
    // The reason for this is that we don't know if the attack will actually do damage, so it can cause more harm than good
    if (!attacker->isFlying
        && BWAPI::Broodwar->getGroundHeight(attacker->getTilePosition()) < BWAPI::Broodwar->getGroundHeight(tilePositionX, tilePositionY))
    {
        return;
    }

    upcomingAttacks.emplace_back(attacker,
                                 bullet,
                                 Players::attackDamage(attacker->player, attacker->type, player, type));
}

void UnitImpl::addUpcomingAttack(const Unit &attacker)
{
    // Probes and air units all have their bullets created on the same frame as their cooldown starts, so nothing is needed for them

    // TODO: Carrier and reaver when we have them (if they even make sense)

    switch (attacker->type)
    {
        case BWAPI::UnitTypes::Protoss_Zealot:
        {
#if UPCOMING_ATTACKS_DEBUG
            CherryVis::log(id) << "Adding upcoming attacks from " << *attacker;
#endif

            // Zealots deal damage twice, once after 2 frames and once after 4 frames
            int damagePerHit = Players::attackDamage(attacker->player, attacker->type, player, type) / 2;
            upcomingAttacks.emplace_back(attacker, 2, damagePerHit);
            upcomingAttacks.emplace_back(attacker, 4, damagePerHit);
            break;
        }
        case BWAPI::UnitTypes::Protoss_Dragoon:
#if UPCOMING_ATTACKS_DEBUG
            CherryVis::log(id) << "Adding upcoming attack from " << *attacker;
#endif

            upcomingAttacks.emplace_back(attacker, 7, Players::attackDamage(attacker->player, attacker->type, player, type));
            break;
        case BWAPI::UnitTypes::Protoss_Dark_Templar:
        case BWAPI::UnitTypes::Protoss_Archon:
#if UPCOMING_ATTACKS_DEBUG
            CherryVis::log(id) << "Adding upcoming attack from " << *attacker;
#endif
            upcomingAttacks.emplace_back(attacker, 4, Players::attackDamage(attacker->player, attacker->type, player, type));
            break;
        case BWAPI::UnitTypes::Protoss_Photon_Cannon:
#if UPCOMING_ATTACKS_DEBUG
            CherryVis::log(id) << "Adding upcoming attack from " << *attacker;
#endif
            upcomingAttacks.emplace_back(attacker, 6, Players::attackDamage(attacker->player, attacker->type, player, type));
            break;
    }
}

void UnitImpl::updateGrid(BWAPI::Unit unit)
{
    auto &grid = Players::grid(unit->getPlayer());

    // Units that have renegaded
    if (unit->getPlayer() != player)
    {
        Players::grid(player).unitDestroyed(type, lastPosition, completed, burrowed, immobile);
        grid.unitCreated(unit->getType(),
                         unit->getPosition(),
                         unit->isCompleted(),
                         unit->isBurrowed(),
                         unit->isStasised() || unit->isLockedDown());
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
        grid.unitDestroyed(type, lastPosition, completed, burrowed, immobile);
        grid.unitCreated(unit->getType(),
                         unit->getPosition(),
                         unit->isCompleted(),
                         unit->isBurrowed(),
                         unit->isStasised() || unit->isLockedDown());
#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitDestroyed (morph) " << lastPosition;
        CherryVis::log(id) << "Grid::unitCreated (morph) " << unit->getPosition();
        Log::Debug() << *this << ": Grid::unitDestroyed (morph) " << lastPosition;
        Log::Debug() << *this << ": Grid::unitCreated (morph) " << unit->getPosition();
#endif
        return;
    }

    // Units that have changed manufactured or carried
    auto nowBeingManufacturedOrCarried = isBeingManufacturedOrCarried();
    if (beingManufacturedOrCarried != nowBeingManufacturedOrCarried)
    {
        // If no longer being manufactured or carried, treat as a new unit
        if (beingManufacturedOrCarried)
        {
            grid.unitCreated(unit->getType(),
                             unit->getPosition(),
                             unit->isCompleted(),
                             unit->isBurrowed(),
                             unit->isStasised() || unit->isLockedDown());
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
        grid.unitDestroyed(type, lastPosition, completed, burrowed, immobile);
#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitDestroyed (carried) " << lastPosition;
        Log::Debug() << *this << ": Grid::unitDestroyed (carried) " << lastPosition;
#endif

        return;
    }
    if (nowBeingManufacturedOrCarried) return; // Don't need to update anything for units that are still being manufactured or carried

    // Units that have completed
    if (!completed && unit->isCompleted())
    {
        grid.unitCompleted(type, lastPosition, burrowed, immobile);
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
        grid.unitDestroyed(type, lastPosition, true, burrowed, immobile);
        grid.unitCreated(unit->getType(),
                         unit->getPosition(),
                         false,
                         unit->isBurrowed(),
                         unit->isStasised() || unit->isLockedDown());
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
        grid.unitCreated(unit->getType(),
                         unit->getPosition(),
                         unit->isCompleted(),
                         unit->isBurrowed(),
                         unit->isStasised() || unit->isLockedDown());
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
        grid.unitDestroyed(type, lastPosition, completed, burrowed, immobile);

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
        grid.unitCreated(unit->getType(),
                         unit->getPosition(),
                         unit->isCompleted(),
                         unit->isBurrowed(),
                         unit->isStasised() || unit->isLockedDown());

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

    // At this point we can ignore any flying buildings
    if (isFlying && type.isBuilding()) return;

    // Units that have moved or changed burrow or immobile state
    if (lastPosition != unit->getPosition() || burrowed != unit->isBurrowed() ||
        immobile != (unit->isStasised() || unit->isLockedDown()))
    {
        grid.unitMoved(unit->getType(),
                       unit->getPosition(),
                       unit->isBurrowed(),
                       unit->isStasised() || unit->isLockedDown(),
                       type,
                       lastPosition,
                       burrowed,
                       immobile);
#if DEBUG_GRID_UPDATES
        CherryVis::log(id) << "Grid::unitMoved " << lastPosition << " to " << unit->getPosition()
                           << "; burrow " << burrowed << " to " << unit->isBurrowed();
        Log::Debug() << *this << ": Grid::unitMoved " << lastPosition << " to " << unit->getPosition()
                              << "; burrow " << burrowed << " to " << unit->isBurrowed();
#endif
    }

    // TODO: Workers in a refinery
}

void UnitImpl::updatePredictedPositions() const
{
    if (predictedPositionsUpdated) return;

    predictedPositionsUpdated = true;

    // Return if we can't predict the movement
    if (!bwapiUnit || !bwapiUnit->exists() || !bwapiUnit->isVisible()) return;

    // Return if the unit isn't moving
    int speed = BWSpeed();
    if (speed == 0) return;

    // Determine the acceleration to use during the prediction
    double topSpeed = Players::unitTopSpeed(player, type);
    int bwTopSpeed = Players::unitBWTopSpeed(player, type);
    if (speed > bwTopSpeed) speed = bwTopSpeed;
    int acceleration;
    if (!bwapiUnit->isAccelerating() || speed >= bwTopSpeed)
    {
        acceleration = 0;
    }
    else
    {
        acceleration = UnitUtil::Acceleration(type, topSpeed);
    }

    // Simulate the positions
    int x = lastPosition.x << 8;
    int y = lastPosition.y << 8;
    int heading = BWHeading();
    for (auto& predictedPosition : predictedPositions)
    {
        Geo::BWMovement(x, y, heading, heading, 0, speed, acceleration, bwTopSpeed);

        predictedPosition.x = x >> 8;
        predictedPosition.y = y >> 8;
    }
}

// Called when updating a unit that is in the fog
// The basic idea is to record the location of enemy combat units when they go into the fog relative to our attack squad's vanguard
// We then use this offset to predict where the enemy unit is, regardless of whether we or the enemy are fleeing
bool UnitImpl::updateSimPosition()
{
    MyUnit vanguard = nullptr;
    double vanguardDirection = 0.0;
    auto getAttackEnemyMainVanguard = [&]()
    {
        auto squad = General::getAttackBaseSquad(Map::getEnemyStartingNatural());
        if (!squad) squad = General::getAttackBaseSquad(Map::getEnemyMain());
        if (!squad) return false;

        auto vanguardCluster = squad->vanguardCluster();
        if (!vanguardCluster) return false;

        vanguard = vanguardCluster->vanguard;
        if (!vanguard) return false;

        auto waypoint = PathFinding::NextGridOrChokeWaypoint(vanguard->lastPosition,
                                                             squad->getTargetPosition(),
                                                             PathFinding::getNavigationGrid(BWAPI::TilePosition(squad->getTargetPosition())),
                                                             3);
        if (waypoint == BWAPI::Positions::Invalid) waypoint = squad->getTargetPosition();

        vanguardDirection = atan2(vanguard->lastPosition.y - waypoint.y, vanguard->lastPosition.x - waypoint.x);

        return true;
    };

    simPosition = lastPosition;
    simPositionValid = lastPositionValid;

    // If the unit just entered the fog, record the offset
    if (lastSeen == currentFrame - 1)
    {
        // First reject units that are not interesting (i.e. are not mobile ground combat units)
        // When we start using corsairs vs. mutas we'll probably need to simulate air unit movement as well
        if (isFlying) return false;
        if (burrowed) return false;
        if (type.isBuilding()) return false;
        if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) return false;
        if (!UnitUtil::IsCombatUnit(type)) return false;

        // Now attempt to get the vanguard unit of the vanguard attack squad cluster
        if (!getAttackEnemyMainVanguard()) return false;

        // Skip units too far away
        auto vanguardDist = vanguard->lastPosition.getDistance(lastPosition);
        if (vanguardDist > 640) return false;

        // Set the distance and angle
        auto angle = atan2(vanguard->lastPosition.y - lastPosition.y, vanguard->lastPosition.x - lastPosition.x);
        offsetToVanguardUnit = std::make_pair(vanguardDist, vanguardDirection - angle);

#if PREDICTED_POSITIONS_DEBUG
        CherryVis::log(id) << "Offset to vanguard @ " << BWAPI::WalkPosition(vanguard->lastPosition)
                           << std::setprecision(3)
                           << ": vanguardDirection=" << (vanguardDirection * 57.2958)
                           << ": angle=" << (angle * 57.2958)
                           << "; dist=" << offsetToVanguardUnit.first
                           << "; diff=" << (offsetToVanguardUnit.second * 57.2958);
#endif

        return true;
    }

    // If we have an offset and a vanguard unit, predict the position
    if (offsetToVanguardUnit.first == 0) return false;
    if (!getAttackEnemyMainVanguard()) return false;

    auto a = vanguardDirection - offsetToVanguardUnit.second;
    simPosition = BWAPI::Position(
            vanguard->lastPosition.x - (int) std::round((double) offsetToVanguardUnit.first * std::cos(a)),
            vanguard->lastPosition.y - (int) std::round((double) offsetToVanguardUnit.first * std::sin(a)));
    simPositionValid = simPosition.isValid();

#if PREDICTED_POSITIONS_DEBUG
    CherryVis::log(id) << "Simulated position: " << BWAPI::WalkPosition(simPosition);
#endif

    return simPositionValid;
}