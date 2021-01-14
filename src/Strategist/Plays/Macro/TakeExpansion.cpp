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
        , requiredBlockClearBuilding(BWAPI::UnitTypes::None)
        , requiredBlockClearBuildingTile(BWAPI::TilePositions::Invalid) {}

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
        if (unit->isTransport() || UnitUtil::CanAttackGround(unit->type))
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
            squad = std::make_shared<AttackBaseSquad>(base);
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

        // Ensure we have at least three units whenever we attack a base
        requestedUnits = std::max(requestedUnits, 3 - (int)squad->getUnits().size());

        // TODO: Request zealot or dragoon when we have that capability
        if (requestedUnits > 0)
        {
            // Only reserve units that have a safe path to the base
            auto gridNodePredicate = [](const NavigationGrid::GridNode &gridNode)
            {
                return gridNode.cost < 300 || Players::grid(BWAPI::Broodwar->enemy()).groundThreat(gridNode.center()) == 0;
            };

            status.unitRequirements.emplace_back(requestedUnits, BWAPI::UnitTypes::Protoss_Dragoon, base->getPosition(), gridNodePredicate, false);
        }
    };

    // If there are enemy combat units, ensure we have an attack squad and cancel anything the builder is doing
    if (enemyValue > 0)
    {
        updateAttackSquad();
        Builder::cancel(depotPosition);
        if (builder)
        {
            Workers::releaseWorker(builder);
            builder = nullptr;
        }

        return;
    }

    // If there is a burrowed Zerg unit, also get an attack squad but allow the builder to continue its work
    if (base->blockedByEnemy && BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
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
    requiredBlockClearBuilding = BWAPI::UnitTypes::None;
    requiredBlockClearBuildingTile = BWAPI::TilePositions::Invalid;
    if (base->blockedByEnemy)
    {
        auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
        if (baseStaticDefenseLocations.first == BWAPI::TilePositions::Invalid ||
            baseStaticDefenseLocations.second.empty())
        {
            status.complete = true;
            return;
        }

        // Ensure the builder stays reserved between buildings
        Workers::reserveWorker(builder);

        // First build the pylon
        auto pylon = Units::myBuildingAt(baseStaticDefenseLocations.first);
        if (!pylon)
        {
            if (!Builder::pendingHere(baseStaticDefenseLocations.first))
            {
                requiredBlockClearBuilding = BWAPI::UnitTypes::Protoss_Pylon;
                requiredBlockClearBuildingTile = baseStaticDefenseLocations.first;
            }

            return;
        }

        // Next build the cannon
        if (pylon->completed)
        {
            auto cannon = Units::myBuildingAt(*baseStaticDefenseLocations.second.begin());
            if (!cannon)
            {
                if (!Builder::pendingHere(*baseStaticDefenseLocations.second.begin()))
                {
                    requiredBlockClearBuilding = BWAPI::UnitTypes::Protoss_Photon_Cannon;
                    requiredBlockClearBuildingTile = *baseStaticDefenseLocations.second.begin();
                }

                return;
            }
        }

        // Falling through to here means the builder doesn't have any work to do

        // If we found the blocking unit, attack it
        if (blocker)
        {
            std::vector<std::pair<MyUnit, Unit>> dummyUnitsAndTargets;
            builder->attackUnit(blocker, dummyUnitsAndTargets);
            return;
        }

        // We didn't find the blocking unit
        // If we have detection on all of the depot's tiles, clear the base being blocked by enemy
        auto detectionGrid = Players::grid(BWAPI::Broodwar->self());
        bool allDetected = true;
        for (int x = depotPosition.x; x < depotPosition.x + 4; x++)
        {
            for (int y = depotPosition.y; y < depotPosition.y + 3; y++)
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

        Log::Get() << "Base @ " << base->getTilePosition() << " is no longer blocked by an enemy unit";
        base->blockedByEnemy = false;
    }

    if (!Builder::isPendingHere(depotPosition))
    {
        Builder::build(BWAPI::UnitTypes::Protoss_Nexus, depotPosition, builder);
    }
}

void TakeExpansion::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Build any buildings needed to clear a blocking unit
    if (requiredBlockClearBuilding != BWAPI::UnitTypes::None)
    {
        auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(requiredBlockClearBuildingTile), 0, 0, 0);
        prioritizedProductionGoals[PRIORITY_DEPOTS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                   requiredBlockClearBuilding,
                                                                   buildLocation,
                                                                   builder);
    }

    // Build an observer if we need one
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.type != BWAPI::UnitTypes::Protoss_Observer) continue;
        if (unitRequirement.count < 1) continue;

        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 unitRequirement.type,
                                                                 unitRequirement.count,
                                                                 1);
    }
}

bool TakeExpansion::constructionStarted() const
{
    return Units::myBuildingAt(depotPosition) != nullptr;
}

void TakeExpansion::disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                            const std::function<void(const MyUnit)> &movableUnitCallback)
{
    Builder::cancel(depotPosition);

    if (builder) Workers::releaseWorker(builder);
}
