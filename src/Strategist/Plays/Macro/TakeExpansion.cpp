#include "TakeExpansion.h"

#include "Builder.h"
#include "Geo.h"
#include "Units.h"
#include "Workers.h"
#include "Players.h"
#include "General.h"
#include "UnitUtil.h"

TakeExpansion::TakeExpansion(Base *baseLoc, int enemyValue)
        : PlayItem((std::ostringstream() << "Take expansion @ " << baseLoc->getTilePosition()).str())
        , baseLoc(baseLoc)
        , enemyValue(enemyValue)
        , depotPosition(baseLoc->getTilePosition())
        , builder(nullptr)
        , buildCannon(false) {}

void TakeExpansion::update()
{
    /* If our builder is dead, release it */
    if (builder && !builder->exists())
    {
        builder = nullptr;
    }

    /* Treat the play as complete when the nexus is finished */
    auto nexus = UnitMgr::myBuildingAt(depotPosition);
    if (nexus)
    {
        if (nexus->completed)
        {
            status.complete = true;
        }

        return;
    }

    /* Cancel the play if the base becomes owned by someone else in the meantime */
    if (!nexus && baseLoc->owner == BWAPI::Broodwar->enemy())
    {
        status.complete = true;
        return;
    }

    /* Update the enemy unit value and look for a blocking unit */
    enemyValue = 0;
    Unit blocker = nullptr;
    bool needDetection = false;
    for (auto &gameUnit : UnitMgr::enemyAtBase(baseLoc))
    {
        /* Count enemies that can currently attack */
        if ((gameUnit->isTransport() || UnitHelper::CanAttackGround(gameUnit->type)) && (!gameUnit->burrowed || gameUnit->type == BWAPI::UnitTypes::Zerg_Lurker))
        {
            enemyValue += CombatSim::unitValue(gameUnit);
            needDetection = needDetection || gameUnit->needsDetection();
        }

        if (gameUnit->isFlying) continue;
        if (Geom::Overlaps(depotPosition, 4, 3, gameUnit->getTilePosition(), 2, 2))
        {
            blocker = gameUnit;

            /* If the unit is unburrowing, assume it is what triggered the base to be blocked */
            if (baseLoc->blockedByEnemy && blocker->rawUnit->getOrder() == BWAPI::Orders::Unburrowing)
            {
                AppLog::Get() << "Base @ " << baseLoc->getTilePosition() << " is probably no longer blocked by an enemy burrowed unit";
                baseLoc->blockedByEnemy = false;
            }
        }
    }

    auto updateAttackSquad = [&]()
    {
        /* Disband the squad if we don't need it any more */
        if (enemyValue == 0)
        {
            if (squad)
            {
                status.removedUnits = squad->getUnits();
                Marshal::removeSquad(squad);
                squad = nullptr;
            }

            return;
        }

        /* Ensure we have the squad */
        if (!squad)
        {
            squad = std::make_shared<AttackBaseSquad>(baseLoc, "Take Expansion");
            Marshal::addSquad(squad);
        }

        /* Check if we need detection */
        auto &detectors = squad->getDetectors();
        if (!needDetection && !detectors.empty())
        {
            status.removedUnits.insert(status.removedUnits.end(), detectors.begin(), detectors.end());
        }
        else if (needDetection && detectors.empty())
        {
            status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Observer, squad->getTargetPosition());

            /* Release the squad units until we get the detector */
            status.removedUnits = squad->getUnits();
            return;
        }

        /* Reserve enough units to attack the base */
        int ourValue = 0;
        for (auto &gameUnit : squad->getUnits())
        {
            ourValue += CombatSim::unitValue(gameUnit);
        }

        int requestedUnits = 0;
        while (ourValue < enemyValue * 2)
        {
            requestedUnits++;
            ourValue += CombatSim::unitValue(BWAPI::UnitTypes::Protoss_Dragoon);
        }

        /* Ensure we have at least two units */
        requestedUnits = std::max(requestedUnits, 2 - (int) squad->getUnits().size());

        /* TODO: Request zealot or dragoon when we have that capability */
        if (requestedUnits > 0)
        {
            status.unitRequirements.emplace_back(requestedUnits, BWAPI::UnitTypes::Protoss_Dragoon, baseLoc->getPosition(), false);
        }
    };

    /* If there are enemy combat units, ensure we have an attack squad and cancel anything the builder is doing */
    if (enemyValue > 0)
    {
        updateAttackSquad();
        Constructor::cancelBase(baseLoc);
        if (builder)
        {
            WorkerMgr::releaseWorker(builder);
            builder = nullptr;
        }

        return;
    }

    /* If there is a burrowed Zerg unit, also get an attack squad but allow the builder to continue its work */
    if (!blocker && baseLoc->blockedByEnemy && BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
    {
        /* Assume it is a zergling */
        enemyValue = CombatSim::unitValue(BWAPI::UnitTypes::Zerg_Zergling);
    }

    updateAttackSquad();

    /* Get a builder if we don't have one */
    if (!builder)
    {
        builder = Constructor::getBuilderUnit(depotPosition, BWAPI::UnitTypes::Protoss_Nexus);
        if (!builder)
        {
            return;
        }
    }

    /* If the enemy base is blocked by a hidden enemy unit, build a cannon to clear it */
    buildCannon = false;
    if (blocker || baseLoc->blockedByEnemy || UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0)
    {
        buildCannon = true;

        /* Ensure the builder stays reserved between buildings */
        WorkerMgr::reserveWorker(builder);

        /* Attack the blocker if we have it and the worker doesn't have anything else to do */
        if (blocker)
        {
            if (!Constructor::hasPendingBuilding(builder) && !blocker->undetected)
            {
                std::static_pointer_cast<MyUnitImpl>(builder)->attackUnit(blocker);
            }

            return;
        }

        /* If we have detection on all of the depot's tiles, clear the base being blocked by enemy */
        auto &detectionGrid = PlayerInfo::grid(BWAPI::Broodwar->self());
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

        if (baseLoc->blockedByEnemy)
        {
            AppLog::Get() << "Base @ " << baseLoc->getTilePosition() << " is no longer blocked by an enemy unit";
            baseLoc->blockedByEnemy = false;
        }
    }

    if (!Constructor::isPendingHere(depotPosition))
    {
        Constructor::build(BWAPI::UnitTypes::Protoss_Nexus, depotPosition, builder);
    }
}

void TakeExpansion::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    if (buildCannon)
    {
        /* Cancel the play if we don't have a cannon location */
        auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(baseLoc);
        if (!baseStaticDefenseLocations.isValid())
        {
            status.complete = true;
            return;
        }

        if (!UnitMgr::myBuildingAt(*baseStaticDefenseLocations.workerDefenseCannons.begin()) &&
            !Constructor::pendingHere(*baseStaticDefenseLocations.workerDefenseCannons.begin()))
        {
            /* Build the pylon if it is not already pending */
            if (!UnitMgr::myBuildingAt(baseStaticDefenseLocations.powerPylon) &&
                !Constructor::pendingHere(baseStaticDefenseLocations.powerPylon))
            {
                auto buildLocation = BuildingPlacement::BuildLocation(BuildBlock::Location(baseStaticDefenseLocations.powerPylon), 0, 0, 0);
                prioProdGoals[PRIORITY_DEPOTS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         label,
                                                                         BWAPI::UnitTypes::Protoss_Pylon,
                                                                         buildLocation,
                                                                         builder);
            }

            auto buildLocation = BuildingPlacement::BuildLocation(BuildBlock::Location(*baseStaticDefenseLocations.workerDefenseCannons.begin()),
                                                                  0,
                                                                  Constructor::framesUntilCompleted(
                                                                          baseStaticDefenseLocations.powerPylon,
                                                                          UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Pylon) + 240),
                                                                  0);
            prioProdGoals[PRIORITY_DEPOTS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                     label,
                                                                     BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                                                     buildLocation,
                                                                     builder);
        }
    }

    /* Build an observer if we need one */
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.type != BWAPI::UnitTypes::Protoss_Observer) continue;
        if (unitRequirement.count < 1) continue;

        prioProdGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 label,
                                                                 unitRequirement.type,
                                                                 unitRequirement.count,
                                                                 1);
    }
}

void TakeExpansion::disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                            const std::function<void(const OwnedUnit)> &movableUnitCallback)
{
    Constructor::cancelBase(baseLoc);

    if (builder && !Constructor::hasPendingBuilding(builder)) WorkerMgr::releaseWorker(builder);

    PlayItem::disband(removedUnitCallback, movableUnitCallback);
}

bool TakeExpansion::cancellable()
{
    /* Don't allow to be cancelled if the builder is very close to the base */
    /* This prevents instability in wanting to take the expansion after the probe has already been sent most of the way */
    if (builder)
    {
        int span = Pathfinder::GetGroundDistance(builder->prevPos, baseLoc->getPosition(), builder->type);
        if (span != -1 && span < 500) return false;
    }

    return UnitMgr::myBuildingAt(depotPosition) == nullptr;
}
