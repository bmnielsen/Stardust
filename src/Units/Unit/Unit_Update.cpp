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
    bool isUndetected(BWAPI::Unit gameUnit)
    {
        return (gameUnit->isBurrowed() || gameUnit->isCloaked() || gameUnit->getType().hasPermanentCloak()) && !gameUnit->isDetected();
    }

    void updateEstimatedCompletionFrame(BWAPI::Unit gameUnit, int &estimatedCompletionFrame)
    {
        if (!gameUnit->getType().isBuilding() || gameUnit->isCompleted())
        {
            estimatedCompletionFrame = -1;
            return;
        }

        /* Don't need to update more than once */
        if (estimatedCompletionFrame != -1) return;

        /* For our own units, this is called on the frame the building started, so we can just use the build time directly */
        if (gameUnit->getPlayer() == BWAPI::Broodwar->self())
        {
            estimatedCompletionFrame = frameNow + UnitHelper::BuildTime(gameUnit->getType());
            return;
        }

        /* For enemy units we need to estimate the start frame based on the current health */
        /* We assume that the building hasn't taken damage */
        /* Buildings start at 10% health and then linearly grow to 100% health */
        /* Then the animation plays (e.g. warp-in for protoss) */

        /* If the building is already at max HP, it is in its animation so we can't determine exactly when it will complete */
        if (gameUnit->getHitPoints() == gameUnit->getType().maxHitPoints())
        {
            estimatedCompletionFrame = frameNow;
            return;
        }

        /* Estimate how far into the build the unit is */
        int initialHitPoints = gameUnit->getType().maxHitPoints() / 10;
        double progress = (double)(gameUnit->getHitPoints() - initialHitPoints) / (double)(gameUnit->getType().maxHitPoints() - initialHitPoints);
        int progressFrame = (int)(progress * (double)gameUnit->getType().buildTime());
        estimatedCompletionFrame = frameNow - progressFrame + UnitHelper::BuildTime(gameUnit->getType());
    }

    BWAPI::TilePosition computeBuildTile(BWAPI::Unit gameUnit)
    {
        if (gameUnit->getType().isBuilding() && !gameUnit->isFlying())
        {
            return gameUnit->getTilePosition();
        }

        return BWAPI::TilePositions::Invalid;
    }
}

UnitImpl::UnitImpl(BWAPI::Unit gameUnit)
        : rawUnit(gameUnit)
        , player(gameUnit->getPlayer())
        , tilePositionX(gameUnit->getPosition().x >> 5U)
        , tilePositionY(gameUnit->getPosition().y >> 5U)
        , buildTile(computeBuildTile(gameUnit))
        , distToTargetPosition(-1)
        , lastCommandFrame(-1)
        , lastSeen(frameNow)
        , lastSeenAttacking(-1)
        , type(gameUnit->getType())
        , ident(gameUnit->getID())
        , prevPos(gameUnit->getPosition())
        , lastPositionValid(true)
        , lastPositionVisible(true)
        , lastAngle(gameUnit->getAngle())
        , beingManufacturedOrCarried(false)
        , frameLastMoved(frameNow)
        , offsetToVanguardUnit(0)
        , simPosition(gameUnit->getPosition())
        , simPositionValid(true)
        , predictedPositionsUpdated(false)
        , bwHeading(0)
        , bwHeadingUpdated(false)
        , bwSpeed(0)
        , bwSpeedUpdated(false)
        , lastHealth(gameUnit->getHitPoints())
        , lastShields(gameUnit->getShields())
        , health(gameUnit->getHitPoints())
        , shields(gameUnit->getShields())
        , lastHealFrame(-1)
        , lastAttackedFrame(-1)
        , completed(gameUnit->isCompleted())
        , estimatedCompletionFrame(-1)
        , isFlying(gameUnit->isFlying())
        , cooldownUntil(frameNow + std::max(gameUnit->getGroundWeaponCooldown(), gameUnit->getAirWeaponCooldown()))
        , stimmedUntil(frameNow + gameUnit->getStimTimer())
        , undetected(isUndetected(gameUnit))
        , immobile(gameUnit->isStasised() || gameUnit->isLockedDown())
        , burrowed(gameUnit->isBurrowed())
        , lastBurrowing(gameUnit->getOrder() == BWAPI::Orders::Burrowing ? frameNow : 0)
        , orderProcessTimer(-1)
        , lastTarget(nullptr) {}

void UnitImpl::created()
{
    predictedPositions.resize(BWAPI::Broodwar->getLatencyFrames() + 2);

    beingManufacturedOrCarried = isBeingManufacturedOrCarried();

    update(rawUnit);

    if (!beingManufacturedOrCarried)
    {
        PlayerInfo::grid(player).unitCreated(type, prevPos, completed, burrowed, immobile);
#if DEBUG_GRID_UPDATES
        Telemetry::log(ident) << "Grid::unitCreated " << prevPos;
        AppLog::Debug() << *this << ": Grid::unitCreated " << prevPos;
#endif

        Telemetry::unitFirstSeen(rawUnit);
    }
}

void UnitImpl::update(BWAPI::Unit gameUnit)
{
    if (!gameUnit || !gameUnit->exists()) return;

    distToTargetPosition = -1;

    /* Convert last command frame to our frame counter */
    if (gameUnit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() - 1)
    {
        lastCommandFrame = frameNow - (BWAPI::Broodwar->getFrameCount() - gameUnit->getLastCommandFrame());
    }

    updateGrid(gameUnit);

    player = gameUnit->getPlayer();
    type = gameUnit->getType();

    lastSeen = frameNow;

    if (prevPos != gameUnit->getPosition())
    {
        frameLastMoved = frameNow;
    }

    tilePositionX = gameUnit->getPosition().x >> 5U;
    tilePositionY = gameUnit->getPosition().y >> 5U;
    prevPos = simPosition = gameUnit->getPosition();
    lastAngle = gameUnit->getAngle();
    offsetToVanguardUnit = 0;
    lastPositionValid = simPositionValid = true;
    lastPositionVisible = true;
    beingManufacturedOrCarried = isBeingManufacturedOrCarried();

    std::fill(predictedPositions.begin(), predictedPositions.end(), prevPos);
    predictedPositionsUpdated = false;

    bwHeadingUpdated = false;
    bwSpeedUpdated = false;

    if (player->getRace() == BWAPI::Races::Terran && gameUnit->isCompleted() && gameUnit->getHitPoints() > lastHealth)
    {
        lastHealFrame = frameNow;
    }

    /* Currently ignoring Terran buildings since they can burn, should track this better if we want to use this for them */
    if (gameUnit->getShields() < lastShields || (gameUnit->getHitPoints() < lastHealth && (player->getRace() != BWAPI::Races::Terran || !type.isBuilding())))
    {
        lastAttackedFrame = frameNow;
    }

    /* Cloaked units show up with 0 hit points and shields, so default to max and otherwise don't touch them */
    undetected = isUndetected(gameUnit);
    if (!undetected)
    {
        lastHealth = gameUnit->getHitPoints();
        lastShields = gameUnit->getShields();
        health = gameUnit->getHitPoints();
        shields = gameUnit->getShields();
    }
    else if (lastHealth == 0)
    {
        lastHealth = gameUnit->getType().maxHitPoints();
        lastShields = gameUnit->getType().maxShields();
        health = gameUnit->getType().maxHitPoints();
        shields = gameUnit->getType().maxShields();
    }

    immobile = gameUnit->isStasised() || gameUnit->isLockedDown();

    burrowed = gameUnit->isBurrowed();
    lastBurrowing = gameUnit->getOrder() == BWAPI::Orders::Burrowing ? frameNow : 0;

    completed = gameUnit->isCompleted();
    updateEstimatedCompletionFrame(gameUnit, estimatedCompletionFrame);

    /* TODO: Track lifted buildings */
    isFlying = gameUnit->isFlying();

    if (gameUnit->isVisible())
    {
        auto cooldown = std::max(gameUnit->getGroundWeaponCooldown(), gameUnit->getAirWeaponCooldown());
        if (cooldown > 0)
        {
            if (cooldown > (cooldownUntil - frameNow + 1))
            {
                lastSeenAttacking = frameNow;
                orderProcessTimer = 0;
            }
            else if (orderProcessTimer == 0 && lastTarget && lastTarget->exists() && lastTarget->lastPositionValid
                && getDistance(lastTarget) < range(lastTarget))
            {
                orderProcessTimer = cooldown;
            }
        }
        cooldownUntil = frameNow + cooldown;
        stimmedUntil = frameNow + gameUnit->getStimTimer();
    }

    /* Process upcoming attacks */
    int upcomingDamage = 0;
    for (auto iterRef = upcomingAttacks.begin(); iterRef != upcomingAttacks.end();)
    {
        /* Remove attacks when they expire */
        if (frameNow >= iterRef->expiryFrame)
        {
#if UPCOMING_ATTACKS_DEBUG
            if (iterRef->attacker)
            {
                Telemetry::log(ident) << "Clearing attack from " << *(iterRef->attacker) << " as it has expired";
            }
#endif
            iterRef = upcomingAttacks.erase(iterRef);
            continue;
        }

        /* Remove bullets when they have hit */
        if (iterRef->bullet)
        {
            if (!iterRef->bullet->exists() || iterRef->bullet->getID() != iterRef->bulletId
                || iterRef->bullet->getPosition().getApproxDistance(iterRef->bullet->getTargetPosition()) == 0)
            {
#if UPCOMING_ATTACKS_DEBUG
                if (iterRef->attacker)
                {
                    Telemetry::log(ident) << "Clearing attack from " << *(iterRef->attacker) << " as bullet has hit";
                }
#endif
                iterRef = upcomingAttacks.erase(iterRef);
                continue;
            }
        }

            /* Clear if the attacker is dead, no longer visible, or out of range */
        else if (!iterRef->unknownAttacker
                 && (!iterRef->attacker || !iterRef->attacker->exists() || !iterRef->attacker->rawUnit->isVisible() || !isInEnemyWeaponRange(iterRef->attacker)))
        {
#if UPCOMING_ATTACKS_DEBUG
            if (iterRef->attacker)
            {
                Telemetry::log(ident) << "Clearing attack from " << *(iterRef->attacker) << " as the attacker is gone";
            }
#endif
            iterRef = upcomingAttacks.erase(iterRef);
            continue;
        }

        upcomingDamage += iterRef->damage;
        iterRef++;
    }

#if UPCOMING_ATTACKS_DEBUG
    if (upcomingDamage > 0)
    {
        Telemetry::log(ident) << "Total value of upcoming attacks is " << upcomingDamage
                           << "; current health is " << health << " (" << shields << ")";
    }
#endif

    /* Do not simulate damage if the unit is being healed */
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
            if (health <= 0) Telemetry::log(ident) << "DOOMED!";
        }
    }
}

void UnitImpl::updateUnitInFog()
{
    bool positionVisible = BWAPI::Broodwar->isVisible(tilePositionX, tilePositionY);

    /* Detect burrowed units we have observed burrowing */
    if (lastBurrowing == frameNow - 1 && !burrowed && positionVisible)
    {
        /* Update grid */
        PlayerInfo::grid(player).unitMoved(type, prevPos, true, immobile, type, prevPos, false, immobile);
#if DEBUG_GRID_UPDATES
        Telemetry::log(ident) << "Grid::unitMoved (observed burrowing)";
    AppLog::Debug() << *this << ": Grid::unitMoved (observed burrowing)";
#endif

        burrowed = true;
    }

    /* Update position simulation */
    auto simPositionSucceeded = updateSimPosition();

    /* Units in fog do not have movement predicted beyond the sim position */
    std::fill(predictedPositions.begin(), predictedPositions.end(), simPosition);
    predictedPositionsUpdated = true;

    /* If we've already detected that this unit has moved from its last known location, skip it */
    if (!lastPositionValid) return;

    /* If the unit has a valid simulated position, treat its last position as invalid */
    if (simPositionSucceeded)
    {
        lastPositionValid = false;
    }

        /* If the last position has been visible for two consecutive frames, the unit is gone */
    else if (positionVisible && lastPositionVisible && lastSeen < (frameNow - 1))
    {
        /* Units that we know have been burrowed at this position might still be there */
        if (burrowed)
        {
            /* Assume the unit is still burrowed here unless we have detection on the position or the unit was doomed before it burrowed */
            auto &grid = PlayerInfo::grid(BWAPI::Broodwar->self());
            if (health > 0 && grid.detection(prevPos) == 0) return;
        }

        lastPositionValid = false;

        /* TODO: Track lifted buildings */
    }

        /* If the unit is a sieged tank, assume it is gone from its last position */
        /* if we haven't seen it in 10 seconds and have a unit that it would otherwise */
        /* fire upon */
    else if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode &&
             lastSeen < (frameNow - 240))
    {
        /* Look for a unit inside the tank's sight range */
        /* TODO: Could also look for a unit inside the tank's weapon range that the enemy can see */
        for (auto gameUnit : BWAPI::Broodwar->self()->getUnits())
        {
            if (!gameUnit->getType().isBuilding() && !gameUnit->isCompleted()) continue;
            if (gameUnit->isLoaded()) continue;
            if (gameUnit->isFlying()) continue;
            if (gameUnit->isStasised()) continue;

            if (Geom::EdgeToEdgeDistance(
                    gameUnit->getType(),
                    gameUnit->getPosition(),
                    BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode,
                    prevPos) <= BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.sightRange())
            {
                lastPositionValid = false;
                break;
            }
        }
    }

    /* For the grid, treat units with invalid last position as destroyed */
    if (!lastPositionValid)
    {
        /* Ignore flying buildings that have moved in the fog */
        if (!type.isBuilding() || !isFlying)
        {
            PlayerInfo::grid(player).unitDestroyed(type, prevPos, completed, burrowed, immobile);
#if DEBUG_GRID_UPDATES
            Telemetry::log(ident) << "Grid::unitDestroyed (!LPV) " << prevPos;
            AppLog::Debug() << *this << ": Grid::unitDestroyed (!LPV) " << prevPos;
#endif
        }
    }

    /* Mark buildings complete in the grid if we expect them to have completed in the fog */
    if (lastPositionValid && !completed && estimatedCompletionFrame != -1 && estimatedCompletionFrame <= frameNow)
    {
        completed = true;
        estimatedCompletionFrame = -1;
        PlayerInfo::grid(player).unitCompleted(type, prevPos, burrowed, immobile);
#if DEBUG_GRID_UPDATES
        Telemetry::log(ident) << "Grid::unitCompleted (FOG) " << prevPos;
        AppLog::Debug() << *this << ": Grid::unitCompleted (FOG) " << prevPos;
#endif
    }

    lastPositionVisible = positionVisible;
}

void UnitImpl::addUpcomingAttack(const Unit &attacker, BWAPI::Bullet bullet)
{
    /* Bullets always remove any existing upcoming attack from this attacker */
    if (attacker)
    {
        for (auto iterRef = upcomingAttacks.begin(); iterRef != upcomingAttacks.end();)
        {
            if (iterRef->attacker == attacker)
            {
#if UPCOMING_ATTACKS_DEBUG
                Telemetry::log(ident) << "Removing upcoming attack from " << *attacker << " because of created bullet";
#endif
                iterRef = upcomingAttacks.erase(iterRef);
            }
            else
                iterRef++;
        }
    }

#if UPCOMING_ATTACKS_DEBUG
    if (attacker)
    {
        Telemetry::log(ident) << "Adding upcoming attack (bullet) from " << *attacker;
    }
    else
    {
        Telemetry::log(ident) << "Adding upcoming attack (bullet) from unknown attacker";
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
    /* Probes and air units all have their bullets created on the same frame as their cooldown starts, so nothing is needed for them */

    /* TODO: Carrier and reaver when we have them (if they even make sense) */

    switch (attacker->type)
    {
        case BWAPI::UnitTypes::Protoss_Zealot:
        {
#if UPCOMING_ATTACKS_DEBUG
            Telemetry::log(ident) << "Adding upcoming attacks from " << *attacker;
#endif

            /* Zealots deal damage twice, once after 2 frames and once after 4 frames */
            int damagePerHit = PlayerInfo::attackDamage(attacker->player, attacker->type, player, type) / 2;
            upcomingAttacks.emplace_back(attacker, 2, damagePerHit);
            upcomingAttacks.emplace_back(attacker, 4, damagePerHit);
            break;
        }
        case BWAPI::UnitTypes::Protoss_Dragoon:
#if UPCOMING_ATTACKS_DEBUG
            Telemetry::log(ident) << "Adding upcoming attack from " << *attacker;
#endif

            upcomingAttacks.emplace_back(attacker, 7, PlayerInfo::attackDamage(attacker->player, attacker->type, player, type));
            break;
        case BWAPI::UnitTypes::Protoss_Dark_Templar:
        case BWAPI::UnitTypes::Protoss_Archon:
#if UPCOMING_ATTACKS_DEBUG
            Telemetry::log(ident) << "Adding upcoming attack from " << *attacker;
#endif
            upcomingAttacks.emplace_back(attacker, 4, PlayerInfo::attackDamage(attacker->player, attacker->type, player, type));
            break;
        case BWAPI::UnitTypes::Protoss_Photon_Cannon:
#if UPCOMING_ATTACKS_DEBUG
            Telemetry::log(ident) << "Adding upcoming attack from " << *attacker;
#endif
            upcomingAttacks.emplace_back(attacker, 6, PlayerInfo::attackDamage(attacker->player, attacker->type, player, type));
            break;
    }
}

void UnitImpl::updateGrid(BWAPI::Unit gameUnit)
{
    auto &grid = PlayerInfo::grid(gameUnit->getPlayer());

    /* Units that have renegaded */
    if (gameUnit->getPlayer() != player)
    {
        PlayerInfo::grid(player).unitDestroyed(type, prevPos, completed, burrowed, immobile);
        grid.unitCreated(gameUnit->getType(),
                         gameUnit->getPosition(),
                         gameUnit->isCompleted(),
                         gameUnit->isBurrowed(),
                         gameUnit->isStasised() || gameUnit->isLockedDown());
#if DEBUG_GRID_UPDATES
        Telemetry::log(ident) << "Grid::unitDestroyed (renegade) " << prevPos;
        Telemetry::log(ident) << "Grid::unitCreated (renegade) " << gameUnit->getPosition();
        AppLog::Debug() << *this << ": Grid::unitDestroyed (renegade) " << prevPos;
        AppLog::Debug() << *this << ": Grid::unitCreated (renegade) " << gameUnit->getPosition();
#endif
        return;
    }

    /* Units that have morphed */
    if (type != gameUnit->getType())
    {
        grid.unitDestroyed(type, prevPos, completed, burrowed, immobile);
        grid.unitCreated(gameUnit->getType(),
                         gameUnit->getPosition(),
                         gameUnit->isCompleted(),
                         gameUnit->isBurrowed(),
                         gameUnit->isStasised() || gameUnit->isLockedDown());
#if DEBUG_GRID_UPDATES
        Telemetry::log(ident) << "Grid::unitDestroyed (morph) " << prevPos;
        Telemetry::log(ident) << "Grid::unitCreated (morph) " << gameUnit->getPosition();
        AppLog::Debug() << *this << ": Grid::unitDestroyed (morph) " << prevPos;
        AppLog::Debug() << *this << ": Grid::unitCreated (morph) " << gameUnit->getPosition();
#endif
        return;
    }

    /* Units that have changed manufactured or carried */
    auto nowBeingManufacturedOrCarried = isBeingManufacturedOrCarried();
    if (beingManufacturedOrCarried != nowBeingManufacturedOrCarried)
    {
        /* If no longer being manufactured or carried, treat as a new unit */
        if (beingManufacturedOrCarried)
        {
            grid.unitCreated(gameUnit->getType(),
                             gameUnit->getPosition(),
                             gameUnit->isCompleted(),
                             gameUnit->isBurrowed(),
                             gameUnit->isStasised() || gameUnit->isLockedDown());
#if DEBUG_GRID_UPDATES
            Telemetry::log(ident) << "Grid::unitCreated (manufactured) " << gameUnit->getPosition();
            AppLog::Debug() << *this << ": Grid::unitCreated (manufactured) " << gameUnit->getPosition();
#endif

            /* If this is the incomplete to complete transition, register in CherryVis */
            if (!completed && gameUnit->isCompleted())
            {
                Telemetry::unitFirstSeen(rawUnit);
            }

            return;
        }

        /* Otherwise treat it as destroyed until it shows up again */
        grid.unitDestroyed(type, prevPos, completed, burrowed, immobile);
#if DEBUG_GRID_UPDATES
        Telemetry::log(ident) << "Grid::unitDestroyed (carried) " << prevPos;
        AppLog::Debug() << *this << ": Grid::unitDestroyed (carried) " << prevPos;
#endif

        return;
    }
    if (nowBeingManufacturedOrCarried) return; // Don't need to update anything for units that are still being manufactured or carried

    /* Units that have completed */
    if (!completed && gameUnit->isCompleted())
    {
        grid.unitCompleted(type, prevPos, burrowed, immobile);
#if DEBUG_GRID_UPDATES
        Telemetry::log(ident) << "Grid::unitCompleted " << gameUnit->getPosition();
        AppLog::Debug() << *this << ": Grid::unitCompleted " << gameUnit->getPosition();
#endif

        /* Fall through as the unit may have moved */
    }

    /* Units that have "incompleted" */
    /* This either means we assumed something completed in the fog that didn't, or a morph is happening (e.g. hydralist to lurker egg) */
    /* In either case we just recreate the unit in the grid */
    if (completed && !gameUnit->isCompleted())
    {
        grid.unitDestroyed(type, prevPos, true, burrowed, immobile);
        grid.unitCreated(gameUnit->getType(),
                         gameUnit->getPosition(),
                         false,
                         gameUnit->isBurrowed(),
                         gameUnit->isStasised() || gameUnit->isLockedDown());
#if DEBUG_GRID_UPDATES
        Telemetry::log(ident) << "Grid::unitDestroyed (incomplete) " << prevPos;
        Telemetry::log(ident) << "Grid::unitCreated (incomplete) " << gameUnit->getPosition();
        AppLog::Debug() << *this << ": Grid::unitDestroyed (incomplete) " << type << "@" << prevPos;
        AppLog::Debug() << *this << ": Grid::unitCreated (incomplete) " << gameUnit->getType() << "@" << gameUnit->getPosition();
#endif
        return;
    }

    /* Units that moved while in the fog and have now reappeared */
    if (!lastPositionValid)
    {
        grid.unitCreated(gameUnit->getType(),
                         gameUnit->getPosition(),
                         gameUnit->isCompleted(),
                         gameUnit->isBurrowed(),
                         gameUnit->isStasised() || gameUnit->isLockedDown());
#if DEBUG_GRID_UPDATES
        Telemetry::log(ident) << "Grid::unitCreated (LPV) " << gameUnit->getPosition();
        AppLog::Debug() << *this << ": Grid::unitCreated (LPV) " << gameUnit->getPosition();
#endif
        return;
    }

    /* Units that have taken off */
    /* We can just treat them as destroyed, as nothing that can take off has an attack */
    if (!isFlying && gameUnit->isFlying())
    {
        grid.unitDestroyed(type, prevPos, completed, burrowed, immobile);

        /* Also affects navigation grids, so tell the map when a building has lifted */
        if (type.isBuilding())
        {
            auto cornerTL = prevPos - (BWAPI::Position(type.tileSize()) / 2);
            Map::onBuildingLifted(type, BWAPI::TilePosition(cornerTL));
        }

#if DEBUG_GRID_UPDATES
        Telemetry::log(ident) << "Grid::unitDestroyed (liftoff) " << prevPos;
        AppLog::Debug() << *this << ": Grid::unitDestroyed (liftoff) " << prevPos;
#endif
        return;
    }

    /* Units that have landed */
    if (isFlying && !gameUnit->isFlying())
    {
        grid.unitCreated(gameUnit->getType(),
                         gameUnit->getPosition(),
                         gameUnit->isCompleted(),
                         gameUnit->isBurrowed(),
                         gameUnit->isStasised() || gameUnit->isLockedDown());

        /* Also affects navigation grids, so tell the map the unit has landed */
        if (type.isBuilding())
        {
            Map::onBuildingLanded(type, gameUnit->getTilePosition());
        }

#if DEBUG_GRID_UPDATES
        Telemetry::log(ident) << "Grid::unitCreated (landed) " << gameUnit->getPosition();
        AppLog::Debug() << *this << ": Grid::unitCreated (landed) " << gameUnit->getPosition();
#endif
        return;
    }

    /* At this point we can ignore any flying buildings */
    if (isFlying && type.isBuilding()) return;

    /* Units that have moved or changed burrow or immobile state */
    if (prevPos != gameUnit->getPosition() || burrowed != gameUnit->isBurrowed() ||
        immobile != (gameUnit->isStasised() || gameUnit->isLockedDown()))
    {
        grid.unitMoved(gameUnit->getType(),
                       gameUnit->getPosition(),
                       gameUnit->isBurrowed(),
                       gameUnit->isStasised() || gameUnit->isLockedDown(),
                       type,
                       prevPos,
                       burrowed,
                       immobile);
#if DEBUG_GRID_UPDATES
        Telemetry::log(ident) << "Grid::unitMoved " << prevPos << " to " << gameUnit->getPosition()
                           << "; burrow " << burrowed << " to " << gameUnit->isBurrowed();
        AppLog::Debug() << *this << ": Grid::unitMoved " << prevPos << " to " << gameUnit->getPosition()
                              << "; burrow " << burrowed << " to " << gameUnit->isBurrowed();
#endif
    }

    /* TODO: Workers in a refinery */
}

void UnitImpl::updatePredictedPositions() const
{
    if (predictedPositionsUpdated) return;

    predictedPositionsUpdated = true;

    /* Return if we can't predict the movement */
    if (!rawUnit || !rawUnit->exists() || !rawUnit->isVisible()) return;

    /* Return if the unit isn't moving */
    int speed = BWSpeed();
    if (speed == 0) return;

    /* Determine the acceleration to use during the prediction */
    double topSpeed = PlayerInfo::unitTopSpeed(player, type);
    int bwTopSpeed = PlayerInfo::unitBWTopSpeed(player, type);
    if (speed > bwTopSpeed) speed = bwTopSpeed;
    int acceleration;
    if (!rawUnit->isAccelerating() || speed >= bwTopSpeed)
    {
        acceleration = 0;
    }
    else
    {
        acceleration = UnitHelper::Acceleration(type, topSpeed);
    }

    /* Simulate the positions */
    int x = prevPos.x << 8;
    int y = prevPos.y << 8;
    int heading = BWHeading();
    for (auto& predictedPosition : predictedPositions)
    {
        Geom::BWMovement(x, y, heading, heading, 0, speed, acceleration, bwTopSpeed);

        predictedPosition.x = x >> 8;
        predictedPosition.y = y >> 8;
        Map::makePositionValid(predictedPosition.x, predictedPosition.y);
    }
}

/* Called when updating a unit that is in the fog */
/* The basic idea is to record the location of enemy combat units when they go into the fog relative to our attack squad's vanguard */
/* We then use this offset to predict where the enemy unit is, regardless of whether we or the enemy are fleeing */
bool UnitImpl::updateSimPosition()
{
    OwnedUnit vanguard = nullptr;
    BWAPI::Position targetPosition;
    auto getAttackEnemyMainVanguard = [&]()
    {
        auto squad = Marshal::getAttackBaseSquad(Map::getEnemyStartingNatural());
        if (!squad) squad = Marshal::getAttackBaseSquad(Map::getEnemyMain());
        if (!squad) return false;

        auto vanguardCluster = squad->vanguardCluster();
        if (!vanguardCluster) return false;

        vanguard = vanguardCluster->vanguard;
        if (!vanguard) return false;

        targetPosition = squad->getTargetPosition();
        return true;
    };

    simPosition = prevPos;
    simPositionValid = lastPositionValid;

    /* If the unit just entered the fog, record the offset */
    if (lastSeen == frameNow - 1)
    {
        /* First reject units that are not interesting (i.e. are not mobile ground combat units) */
        /* When we start using corsairs vs. mutas we'll probably need to simulate air unit movement as well */
        if (isFlying) return false;
        if (burrowed) return false;
        if (type.isBuilding()) return false;
        if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) return false;
        if (!UnitHelper::IsCombatUnit(type)) return false;

        /* Now attempt to get the vanguard unit of the vanguard attack squad cluster */
        if (!getAttackEnemyMainVanguard()) return false;

        /* Skip units too far away */
        auto vanguardDist = vanguard->prevPos.getApproxDistance(prevPos);
        if (vanguardDist > 640) return false;

        /* Set the distance */
        offsetToVanguardUnit = vanguardDist;
        return true;
    }

    /* If we have an offset and a vanguard unit, predict the position */
    if (offsetToVanguardUnit == 0) return false;
    if (!getAttackEnemyMainVanguard()) return false;

    /* Criteria for target position: */
    /* - Must not be visible */
    /* - Must have a navigation grid */
    NavGrid *grid;
    auto validatePosition = [&grid, &targetPosition](BWAPI::Position pt)
    {
        if (BWAPI::Broodwar->isVisible(BWAPI::TilePosition(pt))) return false;

        grid = Pathfinder::getNavigationGrid(pt);
        if (!grid) return false;

        targetPosition = pt;
        return true;
    };
    if (!validatePosition(targetPosition))
    {
        auto baseLoc = Map::getEnemyMain();
        if (!baseLoc || !validatePosition(baseLoc->getPosition()))
        {
            return false;
        }
    }

    /* Find the node where the path enters the fog */
    auto node = &(*grid)[vanguard->prevPos];
    while (node->nextNode && BWAPI::Broodwar->isVisible(node->x, node->y))
    {
        node = node->nextNode;
    }

    /* Detect if the search failed */
    if (!node->nextNode) return false;

    /* Set the position here to start with */
    simPosition = node->midpoint();
    simPositionValid = true;

    /* Try to scale the position further away where appropriate */
    BWAPI::Position vector(BWAPI::TilePosition(node->x - vanguard->tilePositionX, node->y - vanguard->tilePositionY));
    if (Geom::ApproximateDistance(vector.x, 0, vector.y, 0) < offsetToVanguardUnit)
    {
        auto scaledVector = Geom::ScaleVector(vector, offsetToVanguardUnit);
        if (scaledVector != BWAPI::Positions::Invalid)
        {
            auto pt = vanguard->prevPos + scaledVector;
            Map::makePositionValid(pt.x, pt.y);
            if (Map::isWalkable(BWAPI::TilePosition(pt))) simPosition = pt;
        }
    }

#if SIMULATED_POSITIONS_DEBUG
    Telemetry::log(ident) << "Simulated position: " << BWAPI::WalkPosition(simPosition);
#endif
#if SIMULATED_POSITIONS_DRAW
    Telemetry::drawLine(prevPos.x,
                        prevPos.y,
                        simPosition.x,
                        simPosition.y,
                        Telemetry::DrawColor::White,
                        ident);
#endif

    return simPositionValid;
}