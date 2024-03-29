#include "Unit.h"

#include "Geo.h"
#include "Players.h"
#include "Map.h"
#include "General.h"
#include "UnitUtil.h"
#include "Bullets.h"
#include <iomanip>

#include "DebugFlag_GridUpdates.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define UPCOMING_ATTACKS_DEBUG false
#define SIMULATED_POSITIONS_DEBUG false
#define SIMULATED_POSITIONS_DRAW false
#endif

namespace
{
    bool isUndetected(BWAPI::Unit unit)
    {
        return (unit->isBurrowed() || unit->isCloaked() || unit->getType().hasPermanentCloak()) && !unit->isDetected();
    }

    void updateEstimatedCompletionFrame(BWAPI::Unit unit, int &estimatedCompletionFrame)
    {
        if (!unit->getType().isBuilding() || unit->isCompleted())
        {
            estimatedCompletionFrame = -1;
            return;
        }

        // Don't need to update more than once
        if (estimatedCompletionFrame != -1) return;

        // For our own units, this is called on the frame the building started, so we can just use the build time directly
        if (unit->getPlayer() == BWAPI::Broodwar->self())
        {
            estimatedCompletionFrame = currentFrame + UnitUtil::BuildTime(unit->getType());
            return;
        }

        // For enemy units we need to estimate the start frame based on the current health
        // We assume that the building hasn't taken damage
        // Buildings start at 10% health and then linearly grow to 100% health
        // Then the animation plays (e.g. warp-in for protoss)

        // If the building is already at max HP, it is in its animation so we can't determine exactly when it will complete
        if (unit->getHitPoints() == unit->getType().maxHitPoints())
        {
            estimatedCompletionFrame = currentFrame;
            return;
        }

        // Estimate how far into the build the unit is
        int initialHitPoints = unit->getType().maxHitPoints() / 10;
        double progress = (double)(unit->getHitPoints() - initialHitPoints) / (double)(unit->getType().maxHitPoints() - initialHitPoints);
        int progressFrame = (int)(progress * (double)unit->getType().buildTime());
        estimatedCompletionFrame = currentFrame - progressFrame + UnitUtil::BuildTime(unit->getType());
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
        , offsetToVanguardUnit(0)
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
    offsetToVanguardUnit = 0;
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
    updateEstimatedCompletionFrame(unit, estimatedCompletionFrame);

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
            else if (orderProcessTimer == 0 && lastTarget && lastTarget->exists() && lastTarget->lastPositionValid
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
            if (it->attacker)
            {
                CherryVis::log(id) << "Clearing attack from " << *(it->attacker) << " as it has expired";
            }
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
                if (it->attacker)
                {
                    CherryVis::log(id) << "Clearing attack from " << *(it->attacker) << " as bullet has hit";
                }
#endif
                it = upcomingAttacks.erase(it);
                continue;
            }
        }

            // Clear if the attacker is dead, no longer visible, or out of range
        else if (!it->unknownAttacker
                 && (!it->attacker || !it->attacker->exists() || !it->attacker->bwapiUnit->isVisible() || !isInEnemyWeaponRange(it->attacker)))
        {
#if UPCOMING_ATTACKS_DEBUG
            if (it->attacker)
            {
                CherryVis::log(id) << "Clearing attack from " << *(it->attacker) << " as the attacker is gone";
            }
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

    // Update position simulation
    auto simPositionSucceeded = updateSimPosition();

    // Units in fog do not have movement predicted beyond the sim position
    std::fill(predictedPositions.begin(), predictedPositions.end(), simPosition);
    predictedPositionsUpdated = true;

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
    if (attacker)
    {
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
    }

#if UPCOMING_ATTACKS_DEBUG
    if (attacker)
    {
        CherryVis::log(id) << "Adding upcoming attack (bullet) from " << *attacker;
    }
    else
    {
        CherryVis::log(id) << "Adding upcoming attack (bullet) from unknown attacker";
    }
#endif

    int damage = Bullets::upcomingDamage(bullet);
    if (damage > 0)
    {
        upcomingAttacks.emplace_back(attacker, bullet, damage);
    }
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
        Map::makePositionValid(predictedPosition.x, predictedPosition.y);
    }
}

// Called when updating a unit that is in the fog
// The basic idea is to record the location of enemy combat units when they go into the fog relative to our attack squad's vanguard
// We then use this offset to predict where the enemy unit is, regardless of whether we or the enemy are fleeing
bool UnitImpl::updateSimPosition()
{
    MyUnit vanguard = nullptr;
    BWAPI::Position targetPosition;
    auto getAttackEnemyMainVanguard = [&]()
    {
        auto squad = General::getAttackBaseSquad(Map::getEnemyStartingNatural());
        if (!squad) squad = General::getAttackBaseSquad(Map::getEnemyMain());
        if (!squad) return false;

        auto vanguardCluster = squad->vanguardCluster();
        if (!vanguardCluster) return false;

        vanguard = vanguardCluster->vanguard;
        if (!vanguard) return false;

        targetPosition = squad->getTargetPosition();
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
        auto vanguardDist = vanguard->lastPosition.getApproxDistance(lastPosition);
        if (vanguardDist > 640) return false;

        // Set the distance
        offsetToVanguardUnit = vanguardDist;
        return true;
    }

    // If we have an offset and a vanguard unit, predict the position
    if (offsetToVanguardUnit == 0) return false;
    if (!getAttackEnemyMainVanguard()) return false;

    // Criteria for target position:
    // - Must not be visible
    // - Must have a navigation grid
    NavigationGrid *grid;
    auto validatePosition = [&grid, &targetPosition](BWAPI::Position pos)
    {
        if (BWAPI::Broodwar->isVisible(BWAPI::TilePosition(pos))) return false;

        grid = PathFinding::getNavigationGrid(pos);
        if (!grid) return false;

        targetPosition = pos;
        return true;
    };
    if (!validatePosition(targetPosition))
    {
        auto base = Map::getEnemyMain();
        if (!base || !validatePosition(base->getPosition()))
        {
            return false;
        }
    }

    // Find the node where the path enters the fog
    auto node = &(*grid)[vanguard->lastPosition];
    while (node->nextNode && BWAPI::Broodwar->isVisible(node->x, node->y))
    {
        node = node->nextNode;
    }

    // Detect if the search failed
    if (!node->nextNode) return false;

    // Set the position here to start with
    simPosition = node->center();
    simPositionValid = true;

    // Try to scale the position further away where appropriate
    BWAPI::Position vector(BWAPI::TilePosition(node->x - vanguard->tilePositionX, node->y - vanguard->tilePositionY));
    if (Geo::ApproximateDistance(vector.x, 0, vector.y, 0) < offsetToVanguardUnit)
    {
        auto scaledVector = Geo::ScaleVector(vector, offsetToVanguardUnit);
        if (scaledVector != BWAPI::Positions::Invalid)
        {
            auto pos = vanguard->lastPosition + scaledVector;
            Map::makePositionValid(pos.x, pos.y);
            if (Map::isWalkable(BWAPI::TilePosition(pos))) simPosition = pos;
        }
    }

#if SIMULATED_POSITIONS_DEBUG
    CherryVis::log(id) << "Simulated position: " << BWAPI::WalkPosition(simPosition);
#endif
#if SIMULATED_POSITIONS_DRAW
    CherryVis::drawLine(lastPosition.x,
                        lastPosition.y,
                        simPosition.x,
                        simPosition.y,
                        CherryVis::DrawColor::White,
                        id);
#endif

    return simPositionValid;
}