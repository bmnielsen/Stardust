#include "TakeExpansion.h"

#include "Builder.h"
#include "Geo.h"
#include "Units.h"
#include "Workers.h"
#include "Players.h"
#include "General.h"
#include "UnitUtil.h"

TakeExpansion::TakeExpansion(Base *base, int enemyValue)
        : Play((std::ostringstream() << "Take expansion @ " << base->getTilePosition()).str())
        , base(base)
        , enemyValue(enemyValue)
        , depotPosition(base->getTilePosition())
        , builder(nullptr)
        , buildCannon(false) {}

void TakeExpansion::update()
{
    // If our builder is dead, release it
    if (builder && !builder->exists())
    {
        builder = nullptr;
    }

    // Treat the play as complete when the nexus is finished
    auto nexus = Units::myBuildingAt(depotPosition);
    if (nexus)
    {
        if (nexus->completed)
        {
            status.complete = true;
        }

        return;
    }

    // Cancel the play if the base becomes owned by someone else in the meantime
    if (!nexus && base->owner == BWAPI::Broodwar->enemy())
    {
        status.complete = true;
        return;
    }

    // Update the enemy unit value and look for a blocking unit
    enemyValue = 0;
    Unit blocker = nullptr;
    bool needDetection = false;
    for (auto &unit : Units::enemyAtBase(base))
    {
        // Count enemies that can currently attack
        if ((unit->isTransport() || UnitUtil::CanAttackGround(unit->type)) && (!unit->burrowed || unit->type == BWAPI::UnitTypes::Zerg_Lurker))
        {
            enemyValue += CombatSim::unitValue(unit);
            needDetection = needDetection || unit->needsDetection();
        }

        if (unit->isFlying) continue;
        if (Geo::Overlaps(depotPosition, 4, 3, unit->getTilePosition(), 2, 2))
        {
            blocker = unit;

            // If the unit is unburrowing, assume it is what triggered the base to be blocked
            if (base->blockedByEnemy && blocker->bwapiUnit->getOrder() == BWAPI::Orders::Unburrowing)
            {
                Log::Get() << "Base @ " << base->getTilePosition() << " is probably no longer blocked by an enemy burrowed unit";
                base->blockedByEnemy = false;
            }
        }
    }

    auto updateAttackSquad = [&]()
    {
        // Disband the squad if we don't need it any more
        if (enemyValue == 0)
        {
            if (squad)
            {
                status.removedUnits = squad->getUnits();
                General::removeSquad(squad);
                squad = nullptr;
            }

            return;
        }

        // Ensure we have the squad
        if (!squad)
        {
            squad = std::make_shared<AttackBaseSquad>(base, "Take Expansion");
            General::addSquad(squad);
        }

        // Check if we need detection
        auto &detectors = squad->getDetectors();
        if (!needDetection && !detectors.empty())
        {
            status.removedUnits.insert(status.removedUnits.end(), detectors.begin(), detectors.end());
        }
        else if (needDetection && detectors.empty())
        {
            status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Observer, squad->getTargetPosition());

            // Release the squad units until we get the detector
            status.removedUnits = squad->getUnits();
            return;
        }

        // Reserve enough units to attack the base
        int ourValue = 0;
        for (auto &unit : squad->getUnits())
        {
            ourValue += CombatSim::unitValue(unit);
        }

        int requestedUnits = 0;
        while (ourValue < enemyValue * 2)
        {
            requestedUnits++;
            ourValue += CombatSim::unitValue(BWAPI::UnitTypes::Protoss_Dragoon);
        }

        // Ensure we have at least two units
        requestedUnits = std::max(requestedUnits, 2 - (int) squad->getUnits().size());

        // TODO: Request zealot or dragoon when we have that capability
        if (requestedUnits > 0)
        {
            status.unitRequirements.emplace_back(requestedUnits, BWAPI::UnitTypes::Protoss_Dragoon, base->getPosition(), false);
        }
    };

    // If there are enemy combat units, ensure we have an attack squad and cancel anything the builder is doing
    if (enemyValue > 0)
    {
        updateAttackSquad();
        Builder::cancelBase(base);
        if (builder)
        {
            Workers::releaseWorker(builder);
            builder = nullptr;
        }

        return;
    }

    // If there is a burrowed Zerg unit, also get an attack squad but allow the builder to continue its work
    if (!blocker && base->blockedByEnemy && BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
    {
        // Assume it is a zergling
        enemyValue = CombatSim::unitValue(BWAPI::UnitTypes::Zerg_Zergling);
    }

    updateAttackSquad();

    // Get a builder if we don't have one
    if (!builder)
    {
        builder = Builder::getBuilderUnit(depotPosition, BWAPI::UnitTypes::Protoss_Nexus);
        if (!builder)
        {
            return;
        }
    }

    // If the enemy base is blocked by a hidden enemy unit, build a cannon to clear it
    buildCannon = false;
    if (blocker || base->blockedByEnemy)
    {
        buildCannon = true;

        // Ensure the builder stays reserved between buildings
        Workers::reserveWorker(builder);

        // Attack the blocker if we have it and the worker doesn't have anything else to do
        if (blocker)
        {
            if (!Builder::hasPendingBuilding(builder) && !blocker->undetected)
            {
                builder->attackUnit(blocker);
            }

            return;
        }

        // If we have detection on all of the depot's tiles, clear the base being blocked by enemy
        auto detectionGrid = Players::grid(BWAPI::Broodwar->self());
        bool allDetected = true;
        for (int y = depotPosition.y; y < depotPosition.y + 3; y++)
        {
            for (int x = depotPosition.x; x < depotPosition.x + 4; x++)
            {
                BWAPI::TilePosition here(x, y);
                if (detectionGrid.detection(BWAPI::Position(here) + BWAPI::Position(16, 16)) == 0)
                {
                    allDetected = false;
                    goto exitOuterLoop;
                }
            }
        }
        exitOuterLoop:;

        if (!allDetected)
        {
            return;
        }

        if (base->blockedByEnemy)
        {
            Log::Get() << "Base @ " << base->getTilePosition() << " is no longer blocked by an enemy unit";
            base->blockedByEnemy = false;
        }
    }

    if (!Builder::isPendingHere(depotPosition))
    {
        Builder::build(BWAPI::UnitTypes::Protoss_Nexus, depotPosition, builder);
    }
}

void TakeExpansion::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    if (buildCannon)
    {
        // Cancel the play if we don't have a cannon location
        auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
        if (baseStaticDefenseLocations.first == BWAPI::TilePositions::Invalid ||
            baseStaticDefenseLocations.second.empty())
        {
            status.complete = true;
            return;
        }

        if (!Units::myBuildingAt(*baseStaticDefenseLocations.second.begin()) &&
            !Builder::pendingHere(*baseStaticDefenseLocations.second.begin()))
        {
            // Check the status of the pylon
            int framesToPylon = 0;
            auto pylonUnit = Units::myBuildingAt(baseStaticDefenseLocations.first);
            if (pylonUnit)
            {
                framesToPylon = pylonUnit->estimatedCompletionFrame - currentFrame;
            }
            else
            {
                auto pylonBuilding = Builder::pendingHere(baseStaticDefenseLocations.first);
                if (pylonBuilding)
                {
                    framesToPylon = pylonBuilding->expectedFramesUntilCompletion();
                }
                else
                {
                    auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(baseStaticDefenseLocations.first), 0, 0, 0);
                    prioritizedProductionGoals[PRIORITY_DEPOTS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                             label,
                                                                             BWAPI::UnitTypes::Protoss_Pylon,
                                                                             buildLocation,
                                                                             builder);
                }
            }

            auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(*baseStaticDefenseLocations.second.begin()), 0, framesToPylon, 0);
            prioritizedProductionGoals[PRIORITY_DEPOTS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                     label,
                                                                     BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                                                     buildLocation,
                                                                     builder);
        }
    }

    // Build an observer if we need one
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.type != BWAPI::UnitTypes::Protoss_Observer) continue;
        if (unitRequirement.count < 1) continue;

        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 label,
                                                                 unitRequirement.type,
                                                                 unitRequirement.count,
                                                                 1);
    }
}

void TakeExpansion::disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                            const std::function<void(const MyUnit)> &movableUnitCallback)
{
    Builder::cancelBase(base);

    if (builder && !Builder::hasPendingBuilding(builder)) Workers::releaseWorker(builder);

    Play::disband(removedUnitCallback, movableUnitCallback);
}

bool TakeExpansion::cancellable()
{
    // Don't allow to be cancelled if the builder is very close to the base
    // This prevents instability in wanting to take the expansion after the probe has already been sent most of the way
    if (builder)
    {
        int dist = PathFinding::GetGroundDistance(builder->lastPosition, base->getPosition(), builder->type);
        if (dist != -1 && dist < 500) return false;
    }

    return Units::myBuildingAt(depotPosition) == nullptr;
}
