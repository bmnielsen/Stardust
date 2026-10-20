#include "Squad.h"

#include "Map.h"
#include "Geo.h"
#include "Units.h"
#include "Players.h"
#include "Config.h"
#include "FileTools.h"

#include <sstream>

#include "DebugFlag_UnitOrders.h"

/*
 * This file controls High Templar that are assigned to a squad, mirroring the
 * arbiter pattern (Arbiters.cpp).
 *
 * Direction "storm casting" (2026-09-29, batch18 step-0 evidence): SD researched
 * Psionic Storm in 16/20 games and trained ~7 HTs/game yet issued zero storm
 * casts across the whole series - the tech was researched but had no executor.
 * HTs left inside the combat clusters charge the front line as melee filler
 * (IsRangedUnit is false for weaponless units) and die for nothing.
 *
 * Here templars stay behind the vanguard cluster, flee anything that can shoot
 * them, and step up only to cast a storm at a valuable enemy clump, guarded
 * against friendly fire.
 */

namespace
{
    const int STORM_ENERGY_COST = 75;
    const int STORM_RADIUS = 48;
    const int STORM_CAST_RANGE = 288;

    /* Idle templars hold this far behind the vanguard cluster midpoint, away from the target */
    const int STANDOFF_DISTANCE = 128;

    /* Minimum enemy resource value inside the storm circle to be worth 75 energy */
    const int MIN_ENEMY_VALUE = 400;

    /* Friendly unit value in the circle is tolerated up to this fraction of enemy value */
    const double FRIENDLY_FIRE_TOLERANCE = 0.34;

    /* Frames between cast attempts by the same templar (covers command latency) */
    const int CAST_COOLDOWN = 48;

    /* Flee from enemies that can shoot us from this distance */
    const int THREAT_RANGE = 224;
    const int THREAT_WEIGHT = 128;

    int unitValue(const BWAPI::UnitType &type)
    {
        return type.mineralPrice() + type.gasPrice();
    }

    /* Storm does not damage buildings, so they are neither target nor casualty */
    bool stormAffects(const BWAPI::UnitType &type)
    {
        return !type.isBuilding();
    }

    /* The best storm position this templar can reach: an enemy unit position whose */
    /* storm circle covers the most enemy value while keeping friendly fire acceptable */
    struct StormTarget
    {
        BWAPI::Position pos = BWAPI::Positions::Invalid;
        int enemyValue = 0;
        int friendlyValue = 0;
        int enemies = 0;
    };

    StormTarget getStormTarget(const OwnedUnit &templar, int minEnemies)
    {
        StormTarget best;

        for (const auto &enemy : UnitMgr::allEnemy())
        {
            if (!enemy->exists() || !enemy->completed || !enemy->lastPositionValid) continue;
            if (!stormAffects(enemy->type)) continue;
            if (templar->getDistance(enemy) > STORM_CAST_RANGE) continue;

            StormTarget candidate;
            candidate.pos = enemy->prevPos;

            for (const auto &other : UnitMgr::allEnemy())
            {
                if (!other->exists() || !other->completed || !other->lastPositionValid) continue;
                if (!stormAffects(other->type)) continue;
                if (other->prevPos.getApproxDistance(candidate.pos) > STORM_RADIUS) continue;

                candidate.enemyValue += unitValue(other->type);
                candidate.enemies++;
            }

            for (const auto &friendly : UnitMgr::allMine())
            {
                if (!friendly->exists() || !friendly->completed) continue;
                if (friendly->type.isBuilding()) continue;
                if (friendly->type == BWAPI::UnitTypes::Protoss_High_Templar) continue;
                if (friendly->prevPos.getApproxDistance(candidate.pos) > STORM_RADIUS) continue;

                candidate.friendlyValue += unitValue(friendly->type);
            }

            if (candidate.enemies < minEnemies) continue;
            if (candidate.enemyValue < MIN_ENEMY_VALUE) continue;
            if ((double) candidate.friendlyValue > FRIENDLY_FIRE_TOLERANCE * (double) candidate.enemyValue) continue;

            if (candidate.enemyValue > best.enemyValue)
            {
                best = candidate;
            }
        }

        return best;
    }

    /* Where an idle templar loiters: behind the vanguard cluster relative to the target */
    BWAPI::Position desiredPosition(BWAPI::Position vanguardMidpoint, BWAPI::Position targetPosition)
    {
        if (targetPosition.isValid())
        {
            auto scaled = Geom::ScaleVector(vanguardMidpoint - targetPosition, STANDOFF_DISTANCE);
            if (scaled != BWAPI::Positions::Invalid) return vanguardMidpoint + scaled;
        }
        return vanguardMidpoint;
    }
}

void UnitSquad::executeTemplars()
{
    /* Without the tech researched there is nothing to execute; templars still */
    /* follow the vanguard from behind instead of feeding themselves to the front */
    bool stormResearched = PlayerInfo::hasResearched(BWAPI::Broodwar->self(), BWAPI::TechTypes::Psionic_Storm);

    BWAPI::Position vanguardMidpoint = currentVanguardCluster
                                       ? currentVanguardCluster->midpoint
                                       : Map::getMyMain()->getPosition();

    /* At most one cast per squad per frame: storms do not stack, so dumping */
    /* several on the same clump in one frame only wastes energy */
    bool castThisFrame = false;

    for (auto &templar : templars)
    {
        /* Threat: move away from anything that can shoot us */
        int threatX = 0;
        int threatY = 0;
        for (const auto &enemy : UnitMgr::allEnemy())
        {
            if (!enemy->exists() || !enemy->lastPositionValid) continue;
            if (!enemy->canAttack(templar)) continue;
            if (templar->getDistance(enemy) > THREAT_RANGE) continue;

            auto vector = Geom::ScaleVector(templar->prevPos - enemy->prevPos, THREAT_WEIGHT);
            if (vector == BWAPI::Positions::Invalid) continue;

            threatX += vector.x;
            threatY += vector.y;
        }

        if (threatX != 0 || threatY != 0)
        {
            templar->moveTo(templar->prevPos + BWAPI::Position(threatX, threatY), true);
            continue;
        }

        /* Cast if possible */
        if (stormResearched && !castThisFrame
            && templar->energy >= STORM_ENERGY_COST
            && (frameNow - templar->lastCastFrame) >= CAST_COOLDOWN)
        {
            StormTarget target = getStormTarget(templar, Config::stormCastMinEnemies());
            if (target.pos != BWAPI::Positions::Invalid)
            {
#if DEBUG_UNIT_ORDERS
                Telemetry::log(templar->ident) << "Order: Storm at " << BWAPI::WalkPosition(target.pos);
#endif
                templar->rawUnit->useTech(BWAPI::TechTypes::Psionic_Storm, target.pos);
                templar->lastCastFrame = frameNow;
                castThisFrame = true;

                /* HT-pipe diagnostics: release-build channel for cast quality review */
                FileTools::appendLine("shushanking_htpipe",
                                      (std::ostringstream() << "HTPIPE CAST frame=" << frameNow
                                       << " pos=(" << target.pos.x << "," << target.pos.y << ")"
                                       << " enemies=" << target.enemies
                                       << " enemyValue=" << target.enemyValue
                                       << " friendlyValue=" << target.friendlyValue
                                       << " energy=" << templar->energy).str());
                continue;
            }
        }

        /* Otherwise hold behind the vanguard; a charged templar with no worthwhile */
        /* target keeps pace with the vanguard so it is in range when one appears */
        BWAPI::Position targetPos = desiredPosition(vanguardMidpoint, targetPosition);
        if (stormResearched && templar->energy >= STORM_ENERGY_COST)
        {
            targetPos = vanguardMidpoint;
        }
        templar->moveTo(targetPos);
    }
}
