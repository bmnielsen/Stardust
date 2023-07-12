#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;
using namespace UnitCommandTypes;

namespace McRave::Command {

    namespace {

        double grouping(UnitInfo& unit, WalkPosition w) {
            if (unit.isFlying()) {
                if (unit.hasCommander() && (unit.isTargetedBySplash() || unit.isTargetedBySuicide())) {
                    return 1.0 + Position(w).getDistance(unit.getCommander().lock()->getCommandPosition());
                }
            }
            return 1.0;
        }

        double distance(UnitInfo& unit, WalkPosition w) {
            const auto p = Position(w) + Position(4, 4);
            return max(1.0, p.getDistance(unit.getNavigation()));
        }

        double visited(UnitInfo& unit, WalkPosition w) {
            return log(clamp(double(Broodwar->getFrameCount() - Grids::lastVisitedFrame(w)), 100.0, 1000.0));
        }

        double visible(UnitInfo& unit, WalkPosition w) {
            return log(clamp(double(Broodwar->getFrameCount() - Grids::lastVisibleFrame(TilePosition(w))), 100.0, 1000.0));
        }

        double mobility(UnitInfo& unit, WalkPosition w) {
            return unit.isFlying() ? 1.0 : log(10.0 + double(Grids::getMobility(w)));
        }

        double threat(UnitInfo& unit, WalkPosition w, float current = 0.0f)
        {
            const auto p = Position(w) + Position(4, 4);
            if (unit.isTransport()) {
                if (p.getDistance(unit.getNavigation()) < 32.0)
                    return max(0.01f, Grids::getEGroundThreat(w) + Grids::getEAirThreat(w));
                return max(0.01f, Grids::getEAirThreat(w));
            }

            if (unit.isHidden()) {
                if (!Actions::overlapsDetection(unit.unit(), p, PlayerState::Enemy))
                    return 0.01;
            }
            return max(0.01f, unit.getType().isFlyer() ? (Grids::getEAirThreat(w) - current) : (Grids::getEGroundThreat(w) - current));
        }

        double avoidance(UnitInfo& unit, Position p, Position sim)
        {
            return (p.getDistance(sim));
        }

        Position findViablePosition(UnitInfo& unit, Position pstart, int radius, function<double(WalkPosition)> score)
        {
            // Check if this is a viable position for movement
            const auto viablePosition = [&](const WalkPosition& w, Position& p) {
                if (!unit.getType().isFlyer() || unit.getRole() == Role::Transport) {
                    if (Planning::overlapsPlan(unit, p) || !Util::findWalkable(unit, p))
                        return false;
                }
                if (Actions::isInDanger(unit, p))
                    return false;
                return true;
            };

            auto bestPosition = Positions::Invalid;
            auto best = 0.0;
            const auto start = WalkPosition(pstart);

            // Create a box, keep units outside a tile of the edge of the map if it's a flyer
            const auto left = max(0, start.x - radius - unit.getWalkWidth());
            const auto right = min(Broodwar->mapWidth() * 4, start.x + radius + unit.getWalkWidth());
            const auto up = max(0, start.y - radius - unit.getWalkHeight());
            const auto down = min(Broodwar->mapHeight() * 4, start.y + radius + unit.getWalkHeight());

            // Iterate the WalkPositions inside the box
            for (auto x = left; x < right; x++) {
                for (auto y = up; y < down; y++) {
                    const WalkPosition w(x, y);
                    Position p = Position(w) + Position(4, 4);

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
            return bestPosition;
        }
    }

    bool misc(UnitInfo& unit)
    {
        // Unstick a unit
        if (unit.isStuck() || unit.getLocalState() == LocalState::Hold) {
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
                unit.command(Right_Click_Unit, *unit.getTransport().lock());
                return true;
            }
        }

        // If unit is potentially stuck, try to find a manner pylon
        else if (unit.getRole() == Role::Worker && (unit.framesHoldingResource() >= 100 || unit.framesHoldingResource() <= -200)) {
            auto pylon = Util::getClosestUnit(unit.getPosition(), PlayerState::Enemy, [&](auto &u) {
                return u->getType() == UnitTypes::Protoss_Pylon;
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
        if (!unit.hasTarget())
            return false;
        auto unitTarget = unit.getTarget().lock();

        const auto canAttack = [&]() {
            if (!unitTarget->unit()->exists()
                || unitTarget->isHidden())
                return false;
            return unit.canStartAttack() || unit.getRole() == Role::Defender;
        };

        const auto shouldAttack = [&]() {

            // Combat will attack when in range
            if (unit.getRole() == Role::Combat) {
                if (unit.getType() == Zerg_Mutalisk && unit.isWithinRange(*unitTarget) && unit.canStartAttack() && !unit.isWithinAngle(*unitTarget))
                    return false;
                if (unit.attemptingSurround())
                    return false;
                return unit.isWithinRange(*unitTarget) && (unit.getLocalState() == LocalState::Attack || unit.attemptingRunby());
            }

            // Workers will poke damage if near a build job or is threatning our gathering
            if (unit.getRole() == Role::Worker) {
                if (unit.getBuildPosition().isValid()) {
                    const auto topLeft = Position(unit.getBuildPosition());
                    const auto botRight = topLeft + Position(unit.getBuildType().tileWidth() * 32, unit.getBuildType().tileHeight() * 32);
                    return Util::rectangleIntersect(topLeft, botRight, unitTarget->getPosition());
                }
                if (unitTarget->isThreatening() && unit.getUnitsTargetingThis().empty() && unit.isHealthy() && unit.isWithinRange(*unitTarget) && unit.isWithinGatherRange())
                    return true;
            }

            // Defenders will attack when in range
            if (unit.getRole() == Role::Defender)
                return unit.isWithinRange(*unitTarget);

            return false;
        };

        // If unit can move and should attack
        if (canAttack() && shouldAttack()) {
            unit.command(Attack_Unit, *unitTarget);
            return true;
        }
        return false;
    }

    bool approach(UnitInfo& unit)
    {
        if (!unit.hasTarget())
            return false;
        auto unitTarget = unit.getTarget().lock();

        const auto canApproach = [&]() {
            if (unit.getSpeed() <= 0.0
                || unit.canStartAttack())
                return false;
            return true;
        };

        const auto shouldApproach = [&]() {

            if (unit.getRole() == Role::Combat) {

                auto interceptDistance = unit.getInterceptPosition().getDistance(unit.getPosition());

                if (unit.isCapitalShip()
                    || unitTarget->isSuicidal()
                    || unit.isSpellcaster()
                    || unit.getLocalState() == LocalState::Retreat
                    || (unit.getGroundRange() <= 32.0)
                    || (unit.getType() == Zerg_Lurker && !unit.isBurrowed())
                    || (unit.isFlying() && unit.getPosition().getDistance(unitTarget->getPosition()) < 32.0))
                    return false;

                // HACK: Dragoons shouldn't approach Vultures before range upgrade
                if (unit.getType() == Protoss_Dragoon && Broodwar->self()->getUpgradeLevel(UpgradeTypes::Singularity_Charge) == 0)
                    return false;

                if (unit.isTargetedBySuicide())
                    return true;

                // Approach units that are moving away from us
                if ((!unit.isLightAir() || unitTarget->isFlying()) && unit.getInterceptPosition().isValid() && interceptDistance > unit.getPosition().getDistance(unitTarget->getPosition()))
                    return true;

                // If crushing victory, push forward
                if (!unit.isLightAir() && unit.getSimValue() >= 5.0 && unitTarget->getGroundRange() > 32.0)
                    return true;

                if (!unit.isTargetedBySplash() && !unit.isTargetedBySuicide() && (unitTarget->isSplasher() || unitTarget->isSuicidal()))
                    return false;
            }

            // If this units range is lower and target isn't a building
            auto unitRange = (unitTarget->getType().isFlyer() ? unit.getAirRange() : unit.getGroundRange());
            auto enemyRange = (unit.getType().isFlyer() ? unitTarget->getAirRange() : unitTarget->getGroundRange());
            if (!unit.isLightAir() && unitRange < enemyRange && !unitTarget->getType().isBuilding())
                return true;
            return false;
        };

        // If unit can move and should approach
        if (canApproach() && shouldApproach()) {
            if (!unit.isSuicidal() && unitTarget->getSpeed() < unit.getSpeed()) {
                unit.command(Right_Click_Position, unitTarget->getPosition());
                return true;
            }
            else {
                unit.command(Move, unitTarget->getPosition());
                return true;
            }
        }
        return false;
    }

    bool move(UnitInfo& unit)
    {
        auto atHome = Terrain::inTerritory(PlayerState::Self, unit.getPosition());
        auto sim = unit.hasSimTarget() ? unit.getSimTarget().lock()->getPosition() : Positions::Invalid;
        auto current = unit.isFlying() ? Grids::getEAirThreat(unit.getPosition()) : Grids::getEGroundThreat(unit.getPosition());

        const auto scoreFunction = [&](WalkPosition w) {
            const auto p =          Position(w) + Position(4, 4);
            auto score = 0.0;

            if (unit.getRole() == Role::Worker && !atHome)
                score = mobility(unit, w) * grouping(unit, w) / (distance(unit, w) * threat(unit, w));
            else if ((unit.isLightAir() && unit.attemptingRegroup() && sim.isValid()) || unit.attemptingRunby())
                score = mobility(unit, w) * avoidance(unit, p, sim) * grouping(unit, w) / (distance(unit, w));
            else if (unit.isLightAir() && unit.attemptingHarass())
                score = mobility(unit, w) * avoidance(unit, p, sim) * grouping(unit, w) / (distance(unit, w) * threat(unit, w, current));
            else
                score = mobility(unit, w) * grouping(unit, w) / (distance(unit, w));
            return score;
        };

        auto canMove = [&]() {

            // Combats can move as long as they're not already in range of their target
            if (unit.getRole() == Role::Combat) {

                if (unit.attemptingSurround())
                    return true;

                if (unit.hasTarget()) {
                    auto unitTarget = unit.getTarget().lock();
                    if (unit.getType() == Zerg_Mutalisk && unit.isWithinRange(*unitTarget) && unit.canStartAttack() && !unit.isWithinAngle(*unitTarget))
                        return true;
                    if (unitTarget->unit()->exists() && unit.isWithinRange(*unitTarget) && unit.getLocalState() == LocalState::Attack && (unit.getType() != Zerg_Lurker || unit.isBurrowed()))
                        return false;
                }

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

                auto hasMineableResource = false;
                if (unit.hasResource()) {
                    auto resource = unit.getResource().lock();
                    hasMineableResource = resource->getResourceState() == ResourceState::Mineable && resource->unit()->exists();
                }

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

            if (unit.hasTarget() && !unit.attemptingSurround() && !unit.isLightAir() && !unit.isSuicidal() && unit.isWithinRange(*unit.getTarget().lock()))
                return false;

            // Necessary for mutas to not overshoot
            if (unit.getRole() == Role::Combat && !unit.attemptingSurround() && !unit.isSuicidal() && unit.hasTarget() && unit.canStartAttack() && unit.isWithinReach(*unit.getTarget().lock()) && unit.getLocalState() == LocalState::Attack) {
                unit.command(Right_Click_Position, unit.getTarget().lock()->getPosition());
                return true;
            }

            if (unit.attemptingSurround()) {
                unit.command(Move, unit.getSurroundPosition());
                return true;
            }

            if (!unit.getDestinationPath().isReachable()) {
                unit.command(Move, unit.getDestination());
                return true;
            }

            // Find the best position to move to
            auto bestPosition = findViablePosition(unit, unit.getNavigation(), 4, scoreFunction);
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
        auto unitTarget = unit.getTarget().lock();

        const auto scoreFunction = [&](WalkPosition w) {
            const auto score =      (mobility(unit, w) * grouping(unit, w)) / (threat(unit, w));
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
                if (unit.getPosition().getDistance(unitTarget->getPosition()) <= leashRange)
                    return true;
                return false;
            }

            // Special Case: High Templars
            if (unit.getType() == UnitTypes::Protoss_High_Templar)
                return !unit.canStartCast(TechTypes::Psionic_Storm, unitTarget->getPosition());

            // Special Case: Defilers
            if (unit.getType() == UnitTypes::Zerg_Defiler) {
                if (unitTarget->getPlayer() == Broodwar->self())
                    return false;
                else
                    return !unit.canStartCast(TechTypes::Dark_Swarm, unitTarget->getPosition()) && !unit.canStartCast(TechTypes::Plague, unitTarget->getPosition()) && unit.getPosition().getDistance(unitTarget->getPosition()) <= 400.0;
            }

            if (unit.getSpeed() <= 0.0
                || unit.canStartAttack()
                || unit.getType() == Zerg_Lurker)
                return false;
            return true;
        };

        const auto shouldKite = [&]() {
            auto allyRange = (unitTarget->getType().isFlyer() ? unit.getAirRange() : unit.getGroundRange());
            auto enemyRange = (unit.getType().isFlyer() ? unitTarget->getAirRange() : unitTarget->getGroundRange());

            if (unit.getRole() == Role::Worker)
                return Util::getTime() < Time(3, 30) && !unit.getUnitsInRangeOfThis().empty();

            if (unit.getRole() == Role::Combat || unit.getRole() == Role::Scout) {

                if (unit.isTargetedBySuicide() && !unit.isFlying())
                    return false;

                if (unitTarget->isSuicidal() && !unit.isFlying())                                                         // Do kite when the target is a suicidal unit
                    return true;

                if (!unitTarget->canAttackGround() && !unitTarget->canAttackAir() && !unit.getType().isFlyer())      // Don't kite non attackers unless we're a flying unit
                    return false;

                if (unit.getType() == Zerg_Zergling || unit.getType() == Protoss_Corsair
                    || (unit.getGroundRange() < 32.0 && unit.getAirRange() < 32.0))
                    return false;

                if (unit.getType() == Protoss_Zealot && Util::getTime() < Time(4, 00) && Players::PvZ())
                    return false;

                if (unit.getType() == UnitTypes::Protoss_Reaver                                                                 // Reavers: Always kite after shooting
                    || unit.isLightAir()                                                                                        // Do kite when this is a light air unit
                    || allyRange > enemyRange                                                                                   // Do kite when this units range is higher than its target
                    || unit.isCapitalShip()                                                                                     // Do kite when this unit is a capital ship
                    || ((!unit.getUnitsTargetingThis().empty() || !unit.isHealthy()) && allyRange == enemyRange))               // Do kite when this unit is being attacked or isn't healthy
                    return true;
            }
            return false;
        };

        if (shouldKite() && canKite()) {

            // HACK: Drilling with workers. Should add some sort of getClosestResource or fix how PlayerState::Neutral units are stored (we don't store resources in them)
            if (unit.getType().isWorker() && unit.getRole() == Role::Combat) {
                auto closestMineral = Broodwar->getClosestUnit(unitTarget->getPosition(), Filter::IsMineralField, 32);

                if (closestMineral && closestMineral->exists()) {
                    unit.unit()->gather(closestMineral);
                    return true;
                }
                if (unit.hasResource()) {
                    unit.unit()->gather(unit.getResource().lock()->unit());
                    return true;
                }
                return false;
            }

            // If we aren't defending the choke, we just want to move to our mineral hold position
            if (!Combat::defendChoke()) {
                unit.command(Move, Terrain::getDefendPosition());
                return true;
            }

            // If we found a valid position, move to it
            auto bestPosition = findViablePosition(unit, unit.getPosition(), 12, scoreFunction);
            if (bestPosition.isValid()) {
                unit.command(Move, bestPosition);
                return true;
            }
        }
        return false;
    }

    bool defend(UnitInfo& unit)
    {
        bool closeToDefend = Terrain::inTerritory(PlayerState::Self, unit.getPosition())
            || unit.getType().isWorker()
            || unit.getPosition().getDistance(unit.getDestination()) < 160.0
            || (unit.getGoalType() == GoalType::Defend && unit.getPosition().getDistance(unit.getGoal()) < 320.0);

        const auto canDefend = [&]() {
            if (unit.getRole() == Role::Combat)
                return true;
            return false;
        };

        const auto shouldDefend = [&]() {
            if (unit.hasTarget()) {
                auto unitTarget = unit.getTarget().lock();
                if ((unitTarget->isHidden() && unitTarget->isWithinReach(unit))
                    || (unitTarget->isSiegeTank())
                    || (unitTarget->getType() == Zerg_Lurker))
                    return false;
            }
            return closeToDefend && unit.getLocalState() != LocalState::Attack && (unit.getGlobalState() != GlobalState::Attack || unit.getLocalState() == LocalState::Retreat) && !unit.isLightAir() && !unit.attemptingRunby();
        };

        if (canDefend() && shouldDefend()) {
            if (unit.getFormation().isValid() && closeToDefend) {
                unit.command(Move, unit.getFormation());
                return true;
            }
        }
        return false;
    }

    bool explore(UnitInfo& unit)
    {
        const auto scoreFunction = [&](WalkPosition w) {
            auto score = mobility(unit, w) * grouping(unit, w) / (distance(unit, w));
            return score;
        };

        const auto canExplore = [&]() {
            if (unit.getNavigation().isValid() && unit.getSpeed() > 0.0)
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

            auto bestPosition = findViablePosition(unit, unit.getPosition(), 8, scoreFunction);

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
        const auto scoreFunction = [&](WalkPosition w) {
            auto score = 0.0;
            score = (mobility(unit, w) * grouping(unit, w)) / (threat(unit, w) * distance(unit, w));
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

            auto bestPosition = findViablePosition(unit, unit.getNavigation(), 8, scoreFunction);
            if (bestPosition.isValid()) {
                unit.command(Move, bestPosition);
                return true;
            }
            else {
                unit.command(Move, Stations::getDefendPosition(Stations::getClosestRetreatStation(unit)));
                return true;
            }
        }
        return false;
    }

    bool escort(UnitInfo& unit)
    {
        const auto scoreFunction = [&](WalkPosition w) {
            auto score = 1.0 / (threat(unit, w) * distance(unit, w));
            return score;
        };

        // Escorting
        auto shouldEscort = unit.getRole() == Role::Support || unit.getRole() == Role::Transport;
        if (!shouldEscort)
            return false;

        // Try to save on APM
        if (unit.getPosition().getDistance(unit.getDestination()) < 32.0)
            return false;
        if (unit.getPosition().getDistance(unit.getDestination()) < 64.0) {
            unit.command(Right_Click_Position, unit.getDestination());
            return true;
        }

        // If we found a valid position, move to it
        auto bestPosition = findViablePosition(unit, unit.getPosition(), 12, scoreFunction);
        if (bestPosition.isValid()) {
            unit.command(Move, bestPosition);
            return true;
        }
        return false;
    }

    bool transport(UnitInfo& unit)
    {
        auto closestRetreat = Stations::getClosestRetreatStation(unit);

        const auto scoreFunction = [&](WalkPosition w) {
            auto p = Position(w) + Position(4, 4);

            const auto airDist =        max(1.0, p.getDistance(unit.getDestination()));
            const auto grdDist =        max(1.0, BWEB::Map::getGroundDistance(p, unit.getDestination()));
            const auto dist =           grdDist;
            const auto distRetreat =    p.getDistance(Stations::getDefendPosition(closestRetreat));

            if (grdDist == DBL_MAX)
                return 0.0;

            double score = 0.0;
            if (unit.getTransportState() == TransportState::Retreating)
                score = 1.0 / (threat(unit, w) * distRetreat);
            else
                score = 1.0 / (dist);

            for (auto &c : unit.getAssignedCargo()) {
                if (auto &cargo = c.lock()) {

                    // If we just dropped units, we need to make sure not to leave them
                    if (unit.getTransportState() == TransportState::Monitoring) {
                        if (!cargo->unit()->isLoaded() && cargo->getPosition().getDistance(p) > 32.0)
                            return 0.0;
                    }

                    // If we're trying to load
                    if (unit.getTransportState() == TransportState::Loading)
                        score = 1.0 / dist;
                }
            }

            return score;
        };

        auto bestPosition = findViablePosition(unit, unit.getNavigation(), 4, scoreFunction);
        if (bestPosition.isValid()) {
            unit.command(Move, bestPosition);
            return true;
        }
        return false;
    }
}