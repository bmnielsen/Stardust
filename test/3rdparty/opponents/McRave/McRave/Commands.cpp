#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;
using namespace UnitCommandTypes;

namespace McRave::Command {

    namespace {

        double defaultGrouping(UnitInfo& unit, WalkPosition w) {
            return unit.getType().isFlyer() ?
                ((unit.isNearSplash() || unit.isNearSuicide()) ? clamp(Grids::getAAirCluster(w), 0.05f, 1.0f) : 1.00)
                : 1.0;
        }

        double defaultDistance(UnitInfo& unit, WalkPosition w) {
            const auto p = Position(w) + Position(4, 4);
            return max(1.0, p.getDistance(unit.getDestination()));
        }

        double defaultVisited(UnitInfo& unit, WalkPosition w) {
            return log(clamp(double(Broodwar->getFrameCount() - Grids::lastVisitedFrame(w)), 100.0, 1000.0));
        }

        double defaultVisible(UnitInfo& unit, WalkPosition w) {
            return log(clamp(double(Broodwar->getFrameCount() - Grids::lastVisibleFrame(TilePosition(w))), 100.0, 1000.0));
        }

        double defaultMobility(UnitInfo& unit, WalkPosition w) {
            return unit.isFlying() ? 1.0 : log(10.0 + double(Grids::getMobility(w)));
        }

        double defaultThreat(UnitInfo& unit, WalkPosition w)
        {
            const auto p = Position(w) + Position(4, 4);
            if (unit.isTransport()) {
                if (p.getDistance(unit.getDestination()) < 32.0)
                    return max(MIN_THREAT, Grids::getEGroundThreat(w) + Grids::getEAirThreat(w));
                return max(MIN_THREAT, Grids::getEAirThreat(w));
            }

            if (unit.isHidden()) {
                if (!Actions::overlapsDetection(unit.unit(), p, PlayerState::Enemy))
                    return MIN_THREAT;
            }
            return max(MIN_THREAT, unit.getType().isFlyer() ? Grids::getEAirThreat(w) : Grids::getEGroundThreat(w));
        }

        Position findViablePosition(UnitInfo& unit, function<double(WalkPosition)> score)
        {
            // Check if this is a viable position for movement
            const auto viablePosition = [&](const WalkPosition& w, Position p) {
                if (!unit.getType().isFlyer() || unit.getRole() == Role::Transport) {
                    if (Planning::overlapsPlan(unit, p) || Grids::getESplash(w) > 0 || !Util::findWalkable(unit, p))
                        return false;
                }
                if (Actions::isInDanger(unit, p))
                    return false;
                return true;
            };

            auto bestPosition = Positions::Invalid;
            auto best = 0.0;
            const auto start = unit.getWalkPosition();
            const auto radius = unit.getType().isFlyer() ? 12 : clamp(int(unit.getSpeed()), 4, 8) + 8;

            // Create a box, keep units outside a tile of the edge of the map if it's a flyer
            const auto left = max(0, start.x - radius);
            const auto right = min(Broodwar->mapWidth() * 4, start.x + radius + unit.getWalkWidth());
            const auto up = max(0, start.y - radius);
            const auto down = min(Broodwar->mapHeight() * 4, start.y + radius + unit.getWalkHeight());

            // Iterate the WalkPositions inside the box
            for (auto x = left; x < right; x++) {
                for (auto y = up; y < down; y++) {
                    const WalkPosition w(x, y);
                    const Position p = Position(w);

                    if (p.getApproxDistance(unit.getPosition()) > radius * 8)
                        continue;

                    auto current = score(w);
                    if (unit.isLightAir() && unit.getLocalState() != LocalState::Attack) {
                        auto edgePush = clamp(Terrain::getClosestMapEdge(unit.getPosition()).getDistance(p) / 160.0, 0.01, 1.00);
                        auto cornerPush = clamp(Terrain::getClosestMapCorner(unit.getPosition()).getDistance(p) / 320.0, 0.01, 1.00);
                        current = current * cornerPush * edgePush;
                    }

                    if (current > best && viablePosition(w, p)) {
                        best = current;
                        bestPosition = p;
                    }
                }
            }
            Util::findWalkable(unit, bestPosition, unit.unit()->isSelected());
            return bestPosition;
        }
    }

    bool misc(UnitInfo& unit)
    {
        // Unstick a unit
        if (unit.isStuck()) {
            unit.unit()->stop();
            return true;
        }

        // Run irradiated units away
        else if (unit.unit()->isIrradiated()) {
            unit.command(Move, Terrain::getClosestMapCorner(unit.getPosition()));
            return true;
        }

        // If unit has a transport, load into it if we need to
        else if (unit.hasTransport()) {
            if (unit.isRequestingPickup()) {
                unit.command(Right_Click_Unit, unit.getTransport());
                return true;
            }
        }

        // If unit is potentially stuck, try to find a manner pylon
        else if (unit.getRole() == Role::Worker && (unit.framesHoldingResource() >= 100 || unit.framesHoldingResource() <= -200)) {
            auto &pylon = Util::getClosestUnit(unit.getPosition(), PlayerState::Enemy, [&](auto &u) {
                return u.getType() == UnitTypes::Protoss_Pylon;
            });
            if (pylon && pylon->unit() && pylon->unit()->exists() && pylon->getPosition().getDistance(unit.getPosition()) < 128.0) {
                unit.command(Attack_Unit, *pylon);
                return true;
            }
        }
        return false;
    }

    bool attack(UnitInfo& unit)
    {
        const auto canAttack = [&]() {
            if (!unit.hasTarget()
                || !unit.getTarget().unit()->exists()
                || unit.getTarget().isHidden())
                return false;
            return unit.canStartAttack() || unit.getRole() == Role::Defender;
        };

        const auto shouldAttack = [&]() {

            // Combat will attack when in range
            if (unit.getRole() == Role::Combat) {
                if (unit.getType() == Zerg_Mutalisk && unit.isWithinRange(unit.getTarget()) && unit.canStartAttack() && !unit.isWithinAngle(unit.getTarget()))
                    return false;
                if (unit.attemptingSurround())
                    return false;
                return unit.isWithinRange(unit.getTarget()) && unit.getLocalState() == LocalState::Attack;
            }

            // Workers will poke damage if near a build job or is threatning our gathering
            if (unit.getRole() == Role::Worker) {
                if (unit.getBuildPosition().isValid()) {
                    const auto topLeft = Position(unit.getBuildPosition());
                    const auto botRight = topLeft + Position(unit.getBuildType().tileWidth() * 32, unit.getBuildType().tileHeight() * 32);
                    return unit.hasTarget() && Util::rectangleIntersect(topLeft, botRight, unit.getTarget().getPosition());
                }
                if (unit.getTarget().isThreatening() && unit.getTargetedBy().empty() && (unit.isHealthy() || unit.getTargetedBy().empty()) && unit.isWithinRange(unit.getTarget()) && unit.isWithinGatherRange())
                    return true;
            }

            // Defenders will attack when in range
            if (unit.getRole() == Role::Defender)
                return unit.isWithinRange(unit.getTarget());

            // Scouts will attack when in range and at home
            if (unit.getRole() == Role::Scout)
                return unit.isWithinRange(unit.getTarget());

            return false;
        };

        // If unit can move and should attack
        if (canAttack() && shouldAttack()) {
            unit.command(Attack_Unit, unit.getTarget());
            return true;
        }
        return false;
    }

    bool approach(UnitInfo& unit)
    {
        const auto canApproach = [&]() {
            if (!unit.hasTarget()
                || unit.getSpeed() <= 0.0
                || unit.canStartAttack()
                || unit.getType() == Zerg_Lurker)
                return false;
            return true;
        };

        const auto shouldApproach = [&]() {

            if (unit.hasTarget() && unit.isTargetedBySplash())
                return true;

            if (unit.getRole() == Role::Combat) {

                auto interceptDistance = unit.getTarget().getInterceptPosition().getDistance(unit.getPosition());

                if (unit.isCapitalShip()
                    || unit.getTarget().isSuicidal()
                    || unit.isSpellcaster()
                    || unit.getLocalState() == LocalState::Retreat
                    || unit.getType() == Zerg_Zergling
                    || (unit.isFlying() && unit.getPosition().getDistance(unit.getTarget().getPosition()) < 32.0))
                    return false;

                // HACK: Dragoons shouldn't approach Vultures before range upgrade
                if (unit.getType() == Protoss_Dragoon && Broodwar->self()->getUpgradeLevel(UpgradeTypes::Singularity_Charge) == 0)
                    return false;

                // Approach units that are moving away from us
                if ((!unit.isLightAir() || unit.getTarget().isFlying()) && unit.getTarget().getInterceptPosition().isValid() && interceptDistance > unit.getPosition().getDistance(unit.getTarget().getPosition()))
                    return true;

                // If crushing victory, push forward
                if (!unit.isLightAir() && unit.getSimValue() >= 5.0 && unit.getTarget().getGroundRange() > 32.0)
                    return true;
            }

            // If this units range is lower and target isn't a building
            auto unitRange = (unit.getTarget().getType().isFlyer() ? unit.getAirRange() : unit.getGroundRange());
            auto enemyRange = (unit.getType().isFlyer() ? unit.getTarget().getAirRange() : unit.getTarget().getGroundRange());
            if (!unit.isLightAir() && unitRange < enemyRange && !unit.getTarget().getType().isBuilding())
                return true;
            return false;
        };

        // If unit can move and should approach
        if (canApproach() && shouldApproach()) {
            if (unit.getTarget().getSpeed() < unit.getSpeed()) {
                unit.command(Right_Click_Position, unit.getTarget().getPosition());
                return true;
            }
            else {
                unit.command(Move, unit.getTarget().getPosition());
                return true;
            }
        }
        return false;
    }

    bool move(UnitInfo& unit)
    {
        const auto scoreFunction = [&](WalkPosition w) {
            const auto p =          Position(w) + Position(4, 4);
            const auto threat =     defaultThreat(unit, w);
            const auto distance =   defaultDistance(unit, w);
            const auto grouping =   defaultGrouping(unit, w);
            const auto mobility =   defaultMobility(unit, w);
            auto score = 0.0;

            if ((unit.getRole() == Role::Worker && !Terrain::isInAllyTerritory(unit.getTilePosition())) || unit.attemptingRunby())
                score = mobility / (distance * grouping * threat);
            else
                score = mobility / (distance * grouping);
            return score;
        };

        auto canMove = [&]() {

            // Combats can move as long as they're not already in range of their target
            if (unit.getRole() == Role::Combat) {
                if (unit.getType() == Zerg_Mutalisk && unit.hasTarget() && unit.isWithinRange(unit.getTarget()) && unit.canStartAttack() && !unit.isWithinAngle(unit.getTarget()))
                    return true;
                if (unit.attemptingSurround())
                    return true;
                if (unit.hasTarget() && unit.getTarget().unit()->exists() && unit.isWithinRange(unit.getTarget()) && unit.getLocalState() == LocalState::Attack && (unit.getType() != Zerg_Lurker || unit.isBurrowed()))
                    return false;
                return true;
            }

            // Scouts, Workers and Transports can always move
            if (unit.getRole() == Role::Scout
                || unit.getRole() == Role::Worker
                || unit.getRole() == Role::Transport)
                return true;
            return false;
        };

        auto shouldMove = [&]() {

            // Combats should move if we're not retreating
            if (unit.getRole() == Role::Combat)
                return unit.getLocalState() != LocalState::Retreat;

            // Workers should move if they need to get to a new gather or construction job
            if (unit.getRole() == Role::Worker) {
                const auto hasBuildingAssignment = unit.getBuildPosition().isValid() && unit.getBuildType() != UnitTypes::None;
                const auto hasMineableResource = unit.hasResource() && unit.getResource().getResourceState() == ResourceState::Mineable && unit.getResource().unit()->exists();
                return ((hasBuildingAssignment && Workers::shouldMoveToBuild(unit, unit.getBuildPosition(), unit.getBuildType()))
                    || (hasMineableResource && !unit.isWithinGatherRange() && Grids::getEGroundThreat(unit.getPosition()) <= 0.0f && Grids::getAGroundCluster(unit.getPosition()) <= 0.0f))
                    || unit.getGoal().isValid();
            }

            // Scouts and Transports should always move
            if (unit.getRole() == Role::Scout
                || unit.getRole() == Role::Transport)
                return true;
            return false;
        };

        // If unit can move and should move
        if (canMove() && shouldMove()) {

            if (unit.getRole() == Role::Combat && !unit.attemptingSurround() && !unit.isSuicidal() && unit.hasTarget() && unit.canStartAttack() && unit.isWithinReach(unit.getTarget()) && unit.getLocalState() == LocalState::Attack) {
                unit.command(Right_Click_Position, unit.getTarget().getPosition());
                return true;
            }

            if (unit.getRole() == Role::Worker && unit.getBuildType().isValid() && unit.getPosition().getDistance(unit.getDestination()) < 64.0) {
                unit.unit()->move(unit.getDestination());
                return true;
            }

            if (!unit.getDestinationPath().isReachable()) {
                unit.command(Move, unit.getDestination());
                return true;
            }

            // Find the best position to move to
            auto bestPosition = findViablePosition(unit, scoreFunction);
            if (bestPosition.isValid()) {
                unit.command(Move, bestPosition);
                return true;
            }
            else {
                bestPosition = unit.getDestination();
                unit.command(Move, bestPosition);
                return true;
            }
        }
        return false;
    }

    bool kite(UnitInfo& unit)
    {
        // If we don't have a target, we can't kite it
        if (!unit.hasTarget())
            return false;

        const auto scoreFunction = [&](WalkPosition w) {
            const auto p =          Position(w) + Position(4, 4);
            const auto threat =     defaultThreat(unit, w);
            const auto grouping =   defaultGrouping(unit, w);
            const auto mobility =   defaultMobility(unit, w);

            auto distTargeted = 1.0;
            for (auto &t : unit.getTargetedBy()) {
                if (auto target = t.lock()) {
                    auto range = unit.isFlying() ? target->getAirRange() : target->getGroundRange();
                    auto strength = unit.isFlying() ? target->getVisibleAirStrength() : target->getVisibleGroundStrength();
                    auto dist = Util::boxDistance(unit.getType(), p, target->getType(), target->getPosition());
                    distTargeted += max(0.0, dist - range) * strength;
                }
            }

            distTargeted += p.getDistance(unit.getTarget().getPosition());

            const auto score =      (mobility * distTargeted) / (threat * grouping);
            return score;
        };

        const auto canKite = [&]() {

            if (unit.getRole() == Role::Worker)
                return true;

            // Special Case: Carriers
            if (unit.getType() == UnitTypes::Protoss_Carrier) {
                auto leashRange = 320;
                for (auto &interceptor : unit.unit()->getInterceptors()) {
                    if (interceptor->getOrder() != Orders::InterceptorAttack && interceptor->getShields() == interceptor->getType().maxShields() && interceptor->getHitPoints() == interceptor->getType().maxHitPoints() && interceptor->isCompleted())
                        return false;
                }
                if (unit.getPosition().getDistance(unit.getTarget().getPosition()) <= leashRange)
                    return true;
                return false;
            }

            // Special Case: High Templars
            if (unit.getType() == UnitTypes::Protoss_High_Templar)
                return !unit.canStartCast(TechTypes::Psionic_Storm, unit.getTarget().getPosition());

            // Special Case: Defilers
            if (unit.getType() == UnitTypes::Zerg_Defiler) {
                if (unit.getTarget().getPlayer() == Broodwar->self())
                    return false;
                else
                    return !unit.canStartCast(TechTypes::Dark_Swarm, unit.getTarget().getPosition()) && !unit.canStartCast(TechTypes::Plague, unit.getTarget().getPosition()) && unit.getPosition().getDistance(unit.getTarget().getPosition()) <= 400.0;
            }

            if (unit.getSpeed() <= 0.0
                || unit.canStartAttack()
                || unit.getType() == Zerg_Lurker)
                return false;
            return true;
        };

        const auto shouldKite = [&]() {
            auto allyRange = (unit.getTarget().getType().isFlyer() ? unit.getAirRange() : unit.getGroundRange());
            auto enemyRange = (unit.getType().isFlyer() ? unit.getTarget().getAirRange() : unit.getTarget().getGroundRange());

            if (unit.getRole() == Role::Worker && !unit.getTargetedBy().empty() && Util::getTime() < Time(3, 30)) {
                for (auto &t : unit.getTargetedBy()) {
                    if (auto targeter = t.lock()) {
                        if (targeter->isWithinRange(unit))
                            return true;
                    }
                }
            }

            if (unit.getRole() == Role::Combat || unit.getRole() == Role::Scout) {
                auto intercept = unit.getInterceptPosition();
                auto interceptDistance = intercept.getDistance(unit.getPosition());

                if (unit.hasTarget() && unit.isTargetedBySplash())
                    return false;

                if (unit.getTarget().isSuicidal())                                                                             // Do kite when the target is a suicidal unit
                    return true;

                if (!unit.getTarget().canAttackGround() && !unit.getTarget().canAttackAir() && !unit.getType().isFlyer())      // Don't kite buildings unless we're a flying unit
                    return false;

                if (unit.getType() == Zerg_Zergling || unit.getType() == Protoss_Corsair)
                    return false;

                if (unit.getType() == Protoss_Zealot && Util::getTime() < Time(4, 00) && Players::PvZ())
                    return false;

                if (unit.getType() == UnitTypes::Protoss_Reaver                                                                 // Reavers: Always kite after shooting
                    || unit.isLightAir()                                                                                        // Do kite when this is a light air unit
                    || allyRange > enemyRange                                                                                   // Do kite when this units range is higher than its target
                    || unit.isCapitalShip()                                                                                     // Do kite when this unit is a capital ship
                    || ((!unit.getTargetedBy().empty() || !unit.isHealthy()) && allyRange == enemyRange))                       // Do kite when this unit is being attacked or isn't healthy
                    return true;
            }
            return false;
        };

        if (shouldKite() && canKite()) {

            // HACK: Drilling with workers. Should add some sort of getClosestResource or fix how PlayerState::Neutral units are stored (we don't store resources in them)
            if (unit.getType().isWorker() && unit.getRole() == Role::Combat) {
                auto closestMineral = Broodwar->getClosestUnit(unit.getTarget().getPosition(), Filter::IsMineralField, 32);

                if (closestMineral && closestMineral->exists()) {
                    unit.unit()->gather(closestMineral);
                    return true;
                }
                if (unit.hasResource()) {
                    unit.unit()->gather(unit.getResource().unit());
                    return true;
                }
            }

            // If we aren't defending the choke, we just want to move to our mineral hold position
            if (!Combat::defendChoke()) {
                unit.command(Move, Terrain::getDefendPosition());
                return true;
            }

            // If we found a valid position, move to it
            auto bestPosition = findViablePosition(unit, scoreFunction);
            if (bestPosition.isValid()) {
                unit.command(Move, bestPosition);
                return true;
            }
        }
        return false;
    }

    bool defend(UnitInfo& unit)
    {
        const auto scoreFunction = [&](WalkPosition w) {
            const auto distance =   defaultDistance(unit, w);
            const auto mobility =   defaultMobility(unit, w);
            const auto score =      mobility / distance;
            return score;
        };

        bool closeToMainChoke = Position(BWEB::Map::getMainChoke()->Center()).getDistance(unit.getPosition()) < 320.0;
        bool closeToNaturalChoke = Position(BWEB::Map::getNaturalChoke()->Center()).getDistance(unit.getPosition()) < 320.0;
        bool defendingExpansion = unit.getDestination().isValid() && !Terrain::isInEnemyTerritory((TilePosition)unit.getDestination());
        bool closeToDefend = Terrain::isInAllyTerritory(unit.getTilePosition()) || closeToMainChoke || closeToNaturalChoke || unit.getType().isWorker() || (unit.getGoalType() == GoalType::Defend && unit.getPosition().getDistance(unit.getGoal()) < 320.0);

        const auto canDefend = [&]() {
            if (unit.getRole() == Role::Combat)
                return true;
            return false;
        };

        const auto shouldDefend = [&]() {
            if ((unit.hasTarget() && unit.getTarget().isHidden() && unit.getTarget().isWithinReach(unit))
                || (unit.hasTarget() && unit.getTarget().isSiegeTank()))
                return false;

            if (unit.getGoal().isValid() && unit.getGoalType() == GoalType::Defend)
                return closeToDefend;

            return closeToDefend && (unit.isHealthy() || !BuildOrder::isPlayPassive()) && !unit.getGoal().isValid() && unit.getLocalState() != LocalState::Attack && unit.getGlobalState() != GlobalState::Attack;
        };

        if (canDefend() && shouldDefend()) {

            if (Combat::defendChoke()) {

                if (unit.getPosition().getDistance(unit.getDestination()) < 160.0) {
                    unit.command(Move, unit.getDestination());
                    return true;
                }

                // Find the best position to move to
                auto bestPosition = findViablePosition(unit, scoreFunction);
                if (bestPosition.isValid()) {
                    unit.command(Move, bestPosition);
                    return true;
                }
                else {
                    bestPosition = unit.getDestination();
                    unit.command(Move, bestPosition);
                    return true;
                }
            }
            else {
                unit.command(Move, Terrain::getDefendPosition());
                return true;
            }
        }
        return false;
    }

    bool explore(UnitInfo& unit)
    {
        const auto scoreFunction = [&](WalkPosition w) {
            auto threat =     defaultThreat(unit, w);
            const auto distance =   defaultDistance(unit, w) + 640.0; // Padded by rough width or height of starting area
            const auto visible =    defaultVisited(unit, w);
            const auto grouping =   defaultGrouping(unit, w);
            const auto mobility =   defaultMobility(unit, w);

            auto score = 0.0;
            if (!Grids::hasCliffVision(TilePosition(w)))
                score =            mobility / (distance * grouping);
            else
                score =            mobility / (exp(threat) * distance * grouping);

            return score;
        };

        const auto canExplore = [&]() {
            if (unit.getDestination().isValid() && unit.getSpeed() > 0.0)
                return true;
            return false;
        };

        const auto shouldExplore = [&]() {
            if (unit.getRole() == Role::Combat)
                return false;
            else if (unit.getRole() == Role::Scout)
                return unit.getLocalState() != LocalState::Attack;
            return false;
        };

        if (shouldExplore() && canExplore()) {

            // See if there's a way to mineral walk
            if (unit.getType().isWorker() && Terrain::isIslandMap()) {
                for (auto &b : Resources::getMyBoulders()) {
                    auto &boulder = *b;

                    if (boulder.unit()->exists() && boulder.getPosition().getDistance(unit.getPosition()) > 64.0 && boulder.getPosition().getDistance(unit.getDestination()) < unit.getPosition().getDistance(unit.getDestination()) - 64.0) {
                        unit.unit()->gather(boulder.unit());
                        return true;
                    }
                }
            }

            if (unit.getType() == Zerg_Overlord && !Terrain::foundEnemy() && Players::getStrength(PlayerState::Enemy).groundToAir <= 0.0 && Players::getStrength(PlayerState::Enemy).airDefense <= 0.0) {
                unit.command(Move, unit.getDestination());
                return true;
            }

            auto bestPosition = findViablePosition(unit, scoreFunction);
            if (bestPosition.isValid()) {
                unit.command(Move, bestPosition);
                return true;
            }
            else {
                unit.command(Move, unit.getDestination());
                return true;
            }
        }
        return false;
    }

    bool retreat(UnitInfo& unit)
    {
        auto distHome = unit.getPosition().getDistance(unit.getDestination());
        auto percentRetreatHome = unit.hasTarget() ? clamp(unit.getPosition().getDistance(unit.getSimTarget().getPosition()) / unit.getEngageRadius(), 0.0, 1.0) : 1.0;

        const auto scoreFunction = [&](WalkPosition w) {
            const auto p =          Position(w) + Position(4, 4);
            const auto distance =   defaultDistance(unit, w);
            const auto threat =     defaultThreat(unit, w);
            const auto grouping =   defaultGrouping(unit, w);
            const auto mobility =   defaultMobility(unit, w);

            if (unit.isLightAir() && p.getDistance(unit.getDestination()) > unit.getPosition().getDistance(unit.getDestination()) + 32.0)
                return 0.0;

            auto distTargeted = 1.0;
            for (auto &t : unit.getTargetedBy()) {
                if (auto target = t.lock()) {
                    auto range = unit.isFlying() ? target->getAirRange() : target->getGroundRange();
                    auto strength = unit.isFlying() ? target->getVisibleAirStrength() : target->getVisibleGroundStrength();
                    auto dist = Util::boxDistance(unit.getType(), p, target->getType(), target->getPosition());
                    distTargeted += max(0.0, dist - range) * strength;
                }
            }
            if (unit.getTargetedBy().empty() && unit.hasTarget())
                distTargeted = p.getDistance(unit.getTarget().getPosition());

            auto score = (mobility * distTargeted * (1.0 - percentRetreatHome)) / (threat * grouping * distance * percentRetreatHome);

            return score;
        };

        const auto canRetreat = [&]() {
            if (unit.getSpeed() > 0.0)
                return true;
            return false;
        };

        const auto shouldRetreat = [&]() {
            if (unit.getRole() == Role::Combat) {
                if (unit.getLocalState() == LocalState::Retreat)
                    return true;
            }
            if (unit.getRole() == Role::Scout || unit.getRole() == Role::Transport)
                return true;
            return false;
        };

        if (canRetreat() && shouldRetreat()) {

            auto bestPosition = findViablePosition(unit, scoreFunction);
            if (bestPosition.isValid()) {
                unit.command(Move, bestPosition);
                return true;
            }
            else {
                unit.command(Move, Combat::getClosestRetreatPosition(unit));
                return true;
            }
        }
        return false;
    }

    bool escort(UnitInfo& unit)
    {
        auto closestDefense = Util::getClosestUnit(unit.getPosition(), PlayerState::Self, [&](auto &u) {
            return u.canAttackAir();
        });

        const auto scoreFunction = [&](WalkPosition w) {
            const auto p = Position(w) + Position(4, 4);
            const auto threat = defaultThreat(unit, w);
            auto distance = defaultDistance(unit, w);

            // Keep Arbiters at a safe escorting distance
            if (unit.getType() == Protoss_Arbiter && threat > MIN_THREAT)
                distance += 128.0;

            auto score = 1.0 / (threat * distance);
            return score;
        };

        // Try to save on APM
        if (unit.getRole() == Role::Support && unit.getPosition().getDistance(unit.getDestination()) < 32.0)
            return false;

        // Escorting
        auto shouldEscort = unit.getRole() == Role::Support || unit.getRole() == Role::Transport;
        if (!shouldEscort)
            return false;

        // If we found a valid position, move to it
        auto bestPosition = findViablePosition(unit, scoreFunction);
        if (bestPosition.isValid()) {
            unit.command(Move, bestPosition);
            return true;
        }
        return false;
    }

    bool transport(UnitInfo& unit)
    {
        auto cluster = Positions::Invalid;
        auto closestRetreat = Combat::getClosestRetreatPosition(unit);

        const auto scoreFunction = [&](WalkPosition w) {
            auto p = Position(w) + Position(4, 4);

            const auto airDist =        max(1.0, p.getDistance(unit.getDestination()));
            const auto grdDist =        max(1.0, BWEB::Map::getGroundDistance(p, unit.getDestination()));
            const auto dist =           grdDist;
            const auto distRetreat =    p.getDistance(closestRetreat);

            if (grdDist == DBL_MAX)
                return 0.0;

            const auto threat =     unit.getTransportState() == TransportState::Retreating ? defaultThreat(unit, w) : 1.0;
            const auto distance =   unit.getTransportState() == TransportState::Retreating ? distRetreat : dist;
            double score =          1.0 / (threat * distance);

            for (auto &c : unit.getAssignedCargo()) {
                if (auto &cargo = c.lock()) {

                    // If we just dropped units, we need to make sure not to leave them
                    if (unit.getTransportState() == TransportState::Monitoring) {
                        if (!cargo->unit()->isLoaded() && cargo->getPosition().getDistance(p) > 32.0)
                            return 0.0;
                    }

                    // If we're trying to load
                    if (unit.getTransportState() == TransportState::Loading)
                        score = 1.0 / distance;
                }
            }

            return score;
        };

        auto highestCluster = 0.0;
        for (auto &[score, position] : Combat::getCombatClusters()) {
            if (score > highestCluster) {
                highestCluster = score;
                cluster = position;
            }
        }

        auto bestPosition = findViablePosition(unit, scoreFunction);
        if (bestPosition.isValid()) {
            unit.command(Move, bestPosition);
            return true;
        }
        return false;
    }
}