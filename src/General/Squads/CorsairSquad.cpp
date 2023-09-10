#include "CorsairSquad.h"

#include "Units.h"
#include "Players.h"
#include "UnitUtil.h"
#include "Geo.h"
#include "Strategist.h"
#include "Workers.h"

#include "DebugFlag_CombatSim.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define CVIS_LOG_TARGET_SELECTION false
#endif

#if INSTRUMENTATION_ENABLED
#define CVIS_BOARD_VALUE true
#endif

namespace
{
    BWAPI::Position scaledPosition(BWAPI::Position currentPosition, BWAPI::Position vector, int length)
    {
        auto scaledVector = Geo::ScaleVector(vector, length);
        if (scaledVector == BWAPI::Positions::Invalid) return BWAPI::Positions::Invalid;

        return currentPosition + scaledVector;
    }

    void corsairMove(const MyUnit &corsair, BWAPI::Position target)
    {
        // Scale to always move at least our halt distance
        auto moveTarget = scaledPosition(corsair->lastPosition,
                                         target - corsair->lastPosition,
                                         UnitUtil::HaltDistance(BWAPI::UnitTypes::Protoss_Corsair));
        if (!moveTarget.isValid())
        {
            corsair->moveTo(target);
            return;
        }

        // Check for threats
        auto &grid = Players::grid(BWAPI::Broodwar->enemy());
        if (grid.airThreat(moveTarget) == 0)
        {
            corsair->moveTo(moveTarget);
            return;
        }

        // There is a threat - find a default position to move to if we don't find a better option
        moveTarget = scaledPosition(corsair->lastPosition,
                                    corsair->lastPosition - target,
                                    UnitUtil::HaltDistance(BWAPI::UnitTypes::Protoss_Corsair));
        if (!moveTarget.isValid())
        {
            moveTarget = Map::getMyMain()->getPosition();
        }

        // Search for a safe path, stopping if it gets us 10 tiles closer
        auto targetTile = BWAPI::TilePosition(target);
        int currentDist = corsair->getTilePosition().getApproxDistance(targetTile);
        auto avoidThreatTiles = [&grid](BWAPI::TilePosition tile)
        {
            return grid.airThreat((tile.x << 2U) + 2, (tile.y << 2U) + 2) == 0;
        };
        auto closeEnoughToEnd = [&currentDist, &targetTile](BWAPI::TilePosition tile)
        {
            return (currentDist - tile.getApproxDistance(targetTile)) > 10;
        };
        auto path = PathFinding::Search(corsair->getTilePosition(), targetTile, avoidThreatTiles, closeEnoughToEnd, 2);
        if (!path.empty())
        {
            moveTarget = BWAPI::Position(*path.begin()) + BWAPI::Position(16, 16);

            auto scaled = scaledPosition(corsair->lastPosition,
                                         moveTarget - corsair->lastPosition,
                                         UnitUtil::HaltDistance(BWAPI::UnitTypes::Protoss_Corsair));
            if (scaled.isValid())
            {
                moveTarget = scaled;
            }
            else if (path.size() > 2)
            {
                moveTarget = BWAPI::Position(path[2]) + BWAPI::Position(16, 16);
            }
            else if (path.size() > 1)
            {
                moveTarget = BWAPI::Position(path[1]) + BWAPI::Position(16, 16);
            }
        }

        corsair->moveTo(moveTarget);
    }

    void clusterMove(UnitCluster &cluster, BWAPI::Position target)
    {
        cluster.setActivity(UnitCluster::Activity::Moving);

        for (auto &unit : cluster.units)
        {
            corsairMove(unit, target);
        }
    }

    UnitCluster *mainArmyVanguard()
    {
        auto mainArmyPlay = Strategist::getMainArmyPlay();
        if (!mainArmyPlay) return nullptr;

        auto vanguardCluster = mainArmyPlay->getSquad()->vanguardCluster();
        if (!vanguardCluster) return nullptr;

        return vanguardCluster.get();
    }

    BWAPI::Position regroupTarget(UnitCluster &cluster, BWAPI::Position defaultPosition)
    {
        std::vector<BWAPI::Position> positions;

        // Add the main army's vanguard cluster if it has at least one dragoon
        auto vanguard = mainArmyVanguard();
        if (vanguard && vanguard->hasUnitType(BWAPI::UnitTypes::Protoss_Dragoon))
        {
            positions.push_back(vanguard->center);
        }

        // Add powered cannons
        for (auto &cannon : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Photon_Cannon))
        {
            if (!cannon->bwapiUnit->isPowered()) continue;

            positions.push_back(cannon->lastPosition);
        }

        // Return the closest position
        BWAPI::Position best = defaultPosition;
        int bestDist = INT_MAX;
        for (auto &pos : positions)
        {
            int dist = cluster.center.getApproxDistance(pos);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = pos;
            }
        }
        return best;
    }

    bool shouldAttack(UnitCluster &cluster, CombatSimResult &simResult, double aggression = 1.0)
    {
        auto attack = [&]()
        {
            // Always attack if we don't lose anything
            if (simResult.myPercentLost() <= 0.001) return true;

            // Attack in cases where we think we will kill 50% more value than we lose
            if (aggression > 0.99 && simResult.valueGain() > (simResult.initialMine - simResult.finalMine) / 2 &&
                (simResult.percentGain() > -0.05 || simResult.myPercentageOfTotal() > 0.9))
            {
                return true;
            }

            // Attack if we expect to end the fight with a sufficiently larger army and aren't losing an unacceptable percentage of it
            double percentOfTotal = simResult.myPercentageOfTotal() - (0.9 - 0.35 * aggression);
            if (percentOfTotal > 0.0
                && simResult.percentGain() > (-0.05 * aggression - percentOfTotal))
            {
                return true;
            }

            // Attack if the percentage gain, adjusted for aggression and distance factor, is acceptable
            // A percentage gain here means the enemy loses a larger percentage of their army than we do
            if (simResult.percentGain() > (0.2 / aggression)) return true;

            return false;
        };

        bool result = attack();

        simResult.distanceFactor = 1.0;
        simResult.aggression = aggression;

#if DEBUG_COMBATSIM_LOG
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << std::setprecision(2) << "-" << aggression
                         << ": %l=" << simResult.myPercentLost()
                         << "; vg=" << simResult.valueGain()
                         << "; %g=" << simResult.percentGain()
                         << "; %t=" << simResult.myPercentageOfTotal()
                         << (result ? "; ATTACK" : "; RETREAT");
#endif

        return result;
    }

    bool shouldStartAttack(UnitCluster &cluster, CombatSimResult &simResult)
    {
        bool attack = shouldAttack(cluster, simResult);
        cluster.addSimResult(simResult, attack);
        return attack;
    }

    bool shouldContinueAttack(UnitCluster &cluster, CombatSimResult &simResult)
    {
        bool attack = shouldAttack(cluster, simResult, 1.2);
        cluster.addSimResult(simResult, attack);
        if (attack) return true;

        // TODO: Would probably be a good idea to run a retreat sim to see what the consequences of retreating are

        // If this is the first run of the combat sim for this fight, always abort immediately
        if (cluster.recentSimResults.size() < 2)
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": aborting as this is the first sim result";
#endif
            return false;
        }

        CombatSimResult &previousSimResult = cluster.recentSimResults.rbegin()[1].first;

        // If the enemy army strength has increased significantly, abort the attack immediately
        if (simResult.initialEnemy > (int)((double)previousSimResult.initialEnemy * 1.2))
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": aborting as there are now more enemy units";
#endif
            return false;
        }

        int attackFrames;
        int regroupFrames;
        int consecutiveRetreatFrames = UnitCluster::consecutiveSimResults(cluster.recentSimResults, &attackFrames, &regroupFrames, 48);

        // Continue if the sim hasn't been stable for 6 frames
        if (consecutiveRetreatFrames < 6)
        {
#if DEBUG_COMBATSIM_LOG
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing attack as the sim has not yet been stable for 6 frames";
#endif
            return true;
        }

        // Continue if the sim has recommended attacking more than regrouping
        if (attackFrames > regroupFrames)
        {
#if DEBUG_COMBATSIM_LOG
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing attack; regroup=" << regroupFrames
                             << " vs. attack=" << attackFrames;
#endif
            return true;
        }

        // Abort
#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": aborting attack; regroup=" << regroupFrames
                         << " vs. attack=" << attackFrames;
#endif

        return false;
    }

    bool shouldStopRegrouping(UnitCluster &cluster, CombatSimResult &simResult)
    {
        bool attack = shouldAttack(cluster, simResult, 0.8);
        cluster.addSimResult(simResult, attack);
        if (!attack) return false;

        int attackFrames;
        int regroupFrames;
        int consecutiveAttackFrames = UnitCluster::consecutiveSimResults(cluster.recentSimResults, &attackFrames, &regroupFrames, 72);

        // Continue if the sim hasn't been stable for 12 frames
        if (consecutiveAttackFrames < 12)
        {
#if DEBUG_COMBATSIM_LOG
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing regroup as the sim has not yet been stable for 12 frames";
#endif
            return false;
        }

        // Continue if the sim has recommended regrouping more than attacking
        if (regroupFrames > attackFrames)
        {
#if DEBUG_COMBATSIM_LOG
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing regroup; regroup=" << regroupFrames
                             << " vs. attack=" << attackFrames;
#endif
            return false;
        }

        // Continue if our number of units has increased in the past 72 frames.
        // This gives our reinforcements time to link up with the rest of the cluster before engaging.
        int count = 0;
        for (auto it = cluster.recentSimResults.rbegin(); it != cluster.recentSimResults.rend() && count < 72; it++)
        {
            if (simResult.myUnitCount > it->first.myUnitCount)
            {
#if DEBUG_COMBATSIM_LOG
                CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                                 << ": continuing regroup as more friendly units have joined the cluster in the past 72 frames";
#endif
                return false;
            }

            count++;
        }

        // Start the attack
#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": starting attack; regroup=" << regroupFrames
                         << " vs. attack=" << attackFrames;
#endif

        return true;
    }
}

void CorsairSquad::execute()
{
    if (clusters.empty()) return;

    // Helper to set the target position and recompute the clusters so we mark the correct vanguard cluster
    auto ensureTargetPosition = [&](BWAPI::Position position)
    {
        if (targetPosition.getApproxDistance(position) < 32) return;

        targetPosition = position;
        updateClusters();
    };

    // Select the strategy to use for our corsairs based on the targets available
    // Priority is:
    // - Scout enemy main and natural if it is early-game and we haven't scouted it recently
    // - Combat units threatening our bases
    // - Other combat units
    // - Non-combat units not covered by anti-air
    // - Other non-combat units
    // - Scout enemy bases

    auto remainingClusters = clusters;

    // Check if we need to do an early-game scout
    if (Strategist::isWorkerScoutComplete() && currentFrame < 11000
        && !Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Scourge)
        && !Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Hydralisk)
        && !Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Mutalisk))
    {
        Base *baseToScout = nullptr;

        auto enemyMain = Map::getEnemyStartingMain();
        auto enemyNatural = Map::getEnemyStartingNatural();
        if (enemyMain && enemyMain->lastScouted < 7500)
        {
            baseToScout = enemyMain;
        }
        else if (enemyNatural && enemyNatural->lastScouted < 7500)
        {
            baseToScout = enemyNatural;
        }

        if (baseToScout)
        {
            // Scout with the closest cluster
            int bestDist = INT_MAX;
            std::shared_ptr<UnitCluster> bestCluster = nullptr;
            for (const auto &cluster : remainingClusters)
            {
                int dist = cluster->center.getApproxDistance(baseToScout->getPosition());
                if (dist < bestDist)
                {
                    bestDist = dist;
                    bestCluster = cluster;
                }
            }
            if (bestCluster)
            {
                clusterMove(*bestCluster, baseToScout->getPosition());
                remainingClusters.erase(bestCluster);
            }
        }
    }

    if (remainingClusters.empty())
    {
        ensureTargetPosition(Map::getEnemyStartingMain()->getPosition());
#if CVIS_BOARD_VALUE
        CherryVis::setBoardValue("corsairs", "early-game-scout");
#endif
        return;
    }

    // Now scan for targets
    auto &grid = Players::grid(BWAPI::Broodwar->enemy());
    std::unordered_map<Base *, std::set<Unit>> threatenedBases;
    std::set<Unit> combatTargets;
    std::set<Unit> vulnerableNonCombatTargets;
    std::set<std::pair<long, Unit>> nonCombatTargets;
    for (auto &unit : Units::allEnemy())
    {
        if (!unit->isFlying)
        {
#if CVIS_LOG_TARGET_SELECTION
            CherryVis::log(unit->id) << "!flying";
#endif
            continue;
        }
        if (!unit->isAttackable())
        {
#if CVIS_LOG_TARGET_SELECTION
            CherryVis::log(unit->id) << "!attackable";
#endif
            continue;
        }

        // Ignore targets that haven't been seen recently
        // 1 minute threshold for overlords with last position valid, 30 seconds for overlords with last position invalid,
        // 5 seconds for all other targets
        int lastSeenThreshold = 120;
        if (unit->type == BWAPI::UnitTypes::Zerg_Overlord)
        {
            lastSeenThreshold = (unit->lastPositionValid ? 1440 : 720);
        }
        if (unit->lastSeen < (currentFrame - lastSeenThreshold))
        {
#if CVIS_LOG_TARGET_SELECTION
            CherryVis::log(unit->id) << "last seen " << (currentFrame - unit->lastSeen) << " ago";
#endif
            continue;
        }

        // Check if it is threatening one of our bases
        if (unit->canAttackGround())
        {
            bool matched = false;
            for (auto base : Map::getMyBases())
            {
                if (Units::enemyAtBase(base).contains(unit))
                {
                    threatenedBases[base].insert(unit);
                    matched = true;

#if CVIS_LOG_TARGET_SELECTION
                    CherryVis::log(unit->id) << "threatening " << BWAPI::WalkPosition(base->getPosition());
#endif
                    break;
                }
            }
            if (matched) continue;

            combatTargets.insert(unit);

#if CVIS_LOG_TARGET_SELECTION
            CherryVis::log(unit->id) << "combat (ground)";
#endif
            continue;
        }

        // Combat target
        if (unit->canAttackAir())
        {
            combatTargets.insert(unit);

#if CVIS_LOG_TARGET_SELECTION
            CherryVis::log(unit->id) << "combat (air)";
#endif
            continue;
        }

        // Non-combat target
        long threat = grid.airThreat(unit->lastPosition);
        if (threat == 0)
        {
            vulnerableNonCombatTargets.insert(unit);
#if CVIS_LOG_TARGET_SELECTION
            CherryVis::log(unit->id) << "vulnerable non-combat";
#endif
        }
        else
        {
            nonCombatTargets.insert(std::make_pair(threat, unit));
#if CVIS_LOG_TARGET_SELECTION
            CherryVis::log(unit->id) << "non-combat, defended by " << threat;
#endif
        }
    }

    // Logic to determine the least-scouted enemy base (evaluated lazily)
    bool _leastScoutedEnemyBaseComputed = false;
    Base *_leastScoutedEnemyBase = nullptr;
    auto leastScoutedEnemyBase = [&_leastScoutedEnemyBaseComputed, &_leastScoutedEnemyBase, &grid]()
    {
        if (_leastScoutedEnemyBaseComputed) return _leastScoutedEnemyBase;

        _leastScoutedEnemyBaseComputed = true;

        int lastScouted = INT_MAX;
        auto visit = [&lastScouted, &_leastScoutedEnemyBase, &grid](Base *base, int limit = 0)
        {
            // Ignore bases that are covered by anti-air
            if (grid.airThreat(base->getPosition()) > 0) return;

            if (base->lastScouted < lastScouted && base->lastScouted <= (currentFrame - limit))
            {
                lastScouted = base->lastScouted;
                _leastScoutedEnemyBase = base;
            }
        };
        for (auto &base : Map::getEnemyBases())
        {
            visit(base);
        }
        auto &untaken = Map::getUntakenExpansions(BWAPI::Broodwar->enemy());
        if (!untaken.empty()) visit(*untaken.begin(), 1440);

        return _leastScoutedEnemyBase;
    };

    // Pick the targets
    std::set<Unit> targets;
    if (!threatenedBases.empty())
    {
        // Pick the base that has the most workers
        size_t bestWorkers = 0;
        Base *bestBase = nullptr;
        for (auto &baseAndThreats : threatenedBases)
        {
            auto workerCount = Workers::getBaseWorkerCount(baseAndThreats.first);
            if (workerCount > bestWorkers)
            {
                bestWorkers = workerCount;
                bestBase = baseAndThreats.first;
            }
        }
        if (!bestBase)
        {
            bestBase = threatenedBases.begin()->first;
        }

        targets = threatenedBases[bestBase];
        ensureTargetPosition(bestBase->getPosition());

#if CVIS_BOARD_VALUE
        CherryVis::setBoardValue("corsairs", (std::ostringstream() << "defend-" << bestBase->getTilePosition()).str());
#endif

        for (auto &cluster : remainingClusters)
        {
            clusterDefend(*cluster, bestBase, targets);
        }
        return;
    }

    else if (!combatTargets.empty())
    {
        targets = combatTargets;
        ensureTargetPosition((Map::getEnemyMain() ? Map::getEnemyMain() : Map::getMyMain())->getPosition());

#if CVIS_BOARD_VALUE
        CherryVis::setBoardValue("corsairs", "attack-combat");
#endif
    }

    else if (!vulnerableNonCombatTargets.empty())
    {
        targets = vulnerableNonCombatTargets;
        ensureTargetPosition((Map::getEnemyMain() ? Map::getEnemyMain() : Map::getMyMain())->getPosition());

#if CVIS_BOARD_VALUE
        CherryVis::setBoardValue("corsairs", "attack-vulnerable");
#endif
    }

    else if (leastScoutedEnemyBase())
    {
        auto pos = leastScoutedEnemyBase()->getPosition();
        ensureTargetPosition(pos);

#if CVIS_BOARD_VALUE
        CherryVis::setBoardValue("corsairs", (std::ostringstream() << "scout-" << leastScoutedEnemyBase()->getTilePosition()).str());
#endif

        for (auto &cluster : remainingClusters)
        {
            clusterMove(*cluster, pos);
        }
    }

    else if (!nonCombatTargets.empty())
    {
        targets.insert(nonCombatTargets.begin()->second);
        ensureTargetPosition(nonCombatTargets.begin()->second->lastPosition);

#if CVIS_BOARD_VALUE
        CherryVis::setBoardValue("corsairs",
                                 (std::ostringstream() << "attack-noncombat-" << nonCombatTargets.begin()->second->getTilePosition()).str());
#endif
    }

    else
    {
        // Move to an appropriate location:
        // - least-scouted enemy base
        // - main army vanguard cluster
        // - our main
        BWAPI::Position pos;

        auto base = leastScoutedEnemyBase();
        if (base)
        {
            pos = base->getPosition();
        }
        else
        {
            auto vanguard = mainArmyVanguard();
            if (vanguard)
            {
                pos = vanguard->center;
            }
            else
            {
                pos = Map::getMyMain()->getPosition();
            }
        }

        ensureTargetPosition(pos);

#if CVIS_BOARD_VALUE
        CherryVis::setBoardValue("corsairs", (std::ostringstream() << "wait-" << BWAPI::WalkPosition(pos)).str());
#endif

        for (auto &cluster : remainingClusters)
        {
            clusterMove(*cluster, pos);
        }
        return;
    }

    for (auto &cluster : remainingClusters)
    {
        clusterAttack(*cluster, targets);
    }
}

void CorsairSquad::clusterAttack(UnitCluster &cluster, std::set<Unit> &targets)
{
    // Select targets
    auto unitsAndTargets = cluster.selectTargets(targets, cluster.center);

    // If none of our units has a target, move to the target position
    bool hasTarget = false;
    for (const auto &unitAndTarget : unitsAndTargets)
    {
        if (unitAndTarget.second)
        {
            hasTarget = true;
            break;
        }
    }
    if (!hasTarget)
    {
        clusterMove(cluster, targetPosition);
        return;
    }

    // Add nearby ground units that can shoot at us
    auto groundThreat = [](const Unit &unit)
    {
        if (unit->isFlying) return false;
        if (unit->immobile) return false;
        return unit->canAttackAir();
    };
    std::set<Unit> enemyUnits = targets;
    int radius = 640;
    if (cluster.vanguard) radius += cluster.vanguard->getDistance(cluster.center);
    Units::enemyInRadius(enemyUnits, cluster.center, radius, groundThreat);

    // Run combat sim
    auto simResult = cluster.runCombatSim(targetPosition, unitsAndTargets, enemyUnits, detectors);

    // Determine if we should attack
    bool attack;
    switch (cluster.currentActivity)
    {
        case UnitCluster::Activity::Moving:
            attack = shouldStartAttack(cluster, simResult);
            break;
        case UnitCluster::Activity::Attacking:
            attack = shouldContinueAttack(cluster, simResult);
            break;
        case UnitCluster::Activity::Regrouping:
            attack = shouldStopRegrouping(cluster, simResult);
            break;
    }

    if (attack)
    {
        cluster.setActivity(UnitCluster::Activity::Attacking);

        for (auto &unitAndTarget : unitsAndTargets)
        {
            if (!unitAndTarget.first->isReady()) continue;

            if (unitAndTarget.second)
            {
                int dist = unitAndTarget.first->getDistance(unitAndTarget.second);
                if (dist > 320)
                {
                    corsairMove(unitAndTarget.first, unitAndTarget.second->lastPosition);
                }
                else
                {
                    unitAndTarget.first->attackUnit(unitAndTarget.second, unitsAndTargets);
                }
            }
            else
            {
                corsairMove(unitAndTarget.first, targetPosition);
            }
        }
        return;
    }

    // Regroup
    // Corsair move logic takes care of avoiding threats
    cluster.setActivity(UnitCluster::Activity::Regrouping);
    auto pos = regroupTarget(cluster, targetPosition);
    for (auto &unit : cluster.units)
    {
        corsairMove(unit, pos);
    }
}

void CorsairSquad::clusterDefend(UnitCluster &cluster, Base *base, std::set<Unit> &targets)
{
    // If far away from the base, move towards it
    if (cluster.center.getApproxDistance(base->getPosition()) > 1000)
    {
        clusterMove(cluster, base->getPosition());
        return;
    }

    // Select targets
    auto unitsAndTargets = cluster.selectTargets(targets, cluster.center);

    // If none of our units has a target, move to the target position
    bool hasTarget = false;
    for (const auto &unitAndTarget : unitsAndTargets)
    {
        if (unitAndTarget.second)
        {
            hasTarget = true;
            break;
        }
    }
    if (!hasTarget)
    {
        clusterMove(cluster, targetPosition);
        return;
    }

    // Determine if we should attack
    bool attack;

    // If the cluster is covered by static defense, always attack
    auto &grid = Players::grid(BWAPI::Broodwar->self());
    if (grid.staticGroundThreat(cluster.center) > 0)
    {
        attack = true;
    }
    else
    {
        // Run combat sim
        std::set<Unit> enemyUnits = targets;
        int radius = 640;
        if (cluster.vanguard) radius += cluster.vanguard->getDistance(cluster.center);
        Units::enemyInRadius(enemyUnits, cluster.center, radius);
        auto simResult = cluster.runCombatSim(targetPosition, unitsAndTargets, enemyUnits, detectors);

        // Determine if we should attack
        switch (cluster.currentActivity)
        {
            case UnitCluster::Activity::Moving:
                attack = shouldStartAttack(cluster, simResult);
                break;
            case UnitCluster::Activity::Attacking:
                attack = shouldContinueAttack(cluster, simResult);
                break;
            case UnitCluster::Activity::Regrouping:
                attack = shouldStopRegrouping(cluster, simResult);
                break;
        }
    }

    if (attack)
    {
        cluster.setActivity(UnitCluster::Activity::Attacking);

        for (auto &unitAndTarget : unitsAndTargets)
        {
            if (!unitAndTarget.first->isReady()) continue;

            if (unitAndTarget.second)
            {
                int dist = unitAndTarget.first->getDistance(unitAndTarget.second);
                if (dist > 320)
                {
                    corsairMove(unitAndTarget.first, unitAndTarget.second->lastPosition);
                }
                else
                {
                    unitAndTarget.first->attackUnit(unitAndTarget.second, unitsAndTargets);
                }
            }
            else
            {
                corsairMove(unitAndTarget.first, targetPosition);
            }
        }
        return;
    }

    // Regroup
    // Corsair move logic takes care of avoiding threats
    cluster.setActivity(UnitCluster::Activity::Regrouping);
    auto pos = regroupTarget(cluster, targetPosition);
    for (auto &unit : cluster.units)
    {
        corsairMove(unit, pos);
    }
}
