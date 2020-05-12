#include "DefendBase.h"

#include "Strategist.h"
#include "General.h"
#include "Units.h"
#include "UnitUtil.h"
#include "PathFinding.h"
#include "BuildingPlacement.h"
#include "Builder.h"

/*
 * General approach:
 *
 * - If the mineral line is under attack, do worker defense
 * - Otherwise, determine the level of risk to the base.
 * - If the base cannot be saved, evacuate workers
 * - If high risk (enemy units are here or expected to come soon), reserve some units for protection
 * - If at medium risk, get some static defense
 * - If at low risk, do nothing
 */

DefendBase::DefendBase(Base *base)
        : Play((std::ostringstream() << "Defend base @ " << base->getTilePosition()).str())
        , base(base)
        , squad(std::make_shared<DefendBaseSquad>(base))
        , pylon(nullptr)
{
    General::addSquad(squad);

    // Get the static defense locations for this base
    auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
    if (baseStaticDefenseLocations.first != BWAPI::TilePositions::Invalid)
    {
        pylonLocation = baseStaticDefenseLocations.first;
        cannonLocations.assign(baseStaticDefenseLocations.second.begin(), baseStaticDefenseLocations.second.end());

        // Get any existing units - maybe we have had a defend base squad for this base before
        pylon = Units::myBuildingAt(pylonLocation);
        for (auto it = cannonLocations.begin(); it != cannonLocations.end();)
        {
            auto cannon = Units::myBuildingAt(*it);
            if (cannon)
            {
                cannons.push_back(cannon);
                it = cannonLocations.erase(it);
            }
            else
            {
                it++;
            }
        }
    }
}

void DefendBase::update()
{
    // Clear dead static defense buildings
    if (pylon && !pylon->exists()) pylon = nullptr;
    for (auto it = cannons.begin(); it != cannons.end();)
    {
        if ((*it)->exists())
        {
            it++;
        }
        else
        {
            cannonLocations.emplace_front((*it)->getTilePosition());
            it = cannons.erase(it);
        }
    }

    // Check for static defense buildings that have started
    if (!pylon)
    {
        auto pendingBuilding = Builder::pendingHere(pylonLocation);
        if (pendingBuilding) pylon = pendingBuilding->unit;
    }
    for (auto it = cannonLocations.begin(); it != cannonLocations.end();)
    {
        auto pendingBuilding = Builder::pendingHere(*it);
        if (pendingBuilding && pendingBuilding->unit)
        {
            cannons.push_back(pendingBuilding->unit);
            it = cannonLocations.erase(it);
        }
        else
        {
            it++;
        }
    }

    // Gather enemy units threatening the base
    enemyThreats.clear();
    int enemyValue = 0;
    bool requireDragoons = false;
    for (const auto &unit : Units::allEnemy())
    {
        if (!unit->lastPositionValid) continue;
        if (!UnitUtil::IsCombatUnit(unit->type) && unit->lastSeenAttacking < (BWAPI::Broodwar->getFrameCount() - 120)) continue;
        if (!unit->isTransport() && !UnitUtil::CanAttackGround(unit->type)) continue;
        if (!unit->type.isBuilding() && unit->lastSeen < (BWAPI::Broodwar->getFrameCount() - 120)) continue;

        int dist = unit->isFlying
                   ? unit->lastPosition.getApproxDistance(base->getPosition())
                   : PathFinding::GetGroundDistance(unit->lastPosition, base->getPosition(), unit->type);
        if (dist == -1 || dist > 1000) continue;
        if (dist > 500)
        {
            auto predictedPosition = unit->predictPosition(5);
            int predictedDist = unit->isFlying
                                ? predictedPosition.getApproxDistance(base->getPosition())
                                : PathFinding::GetGroundDistance(predictedPosition, base->getPosition(), unit->type);
            if (predictedDist > dist) continue;
        }

        enemyThreats.push_back(unit);
        enemyValue += CombatSim::unitValue(unit);

        // Require dragoons against certain enemy types
        requireDragoons = requireDragoons ||
                          (unit->isFlying ||
                           unit->type == BWAPI::UnitTypes::Protoss_Dragoon ||
                           unit->type == BWAPI::UnitTypes::Terran_Vulture);
    }

    // Release any units in the squad if they are no longer required
    if (enemyValue > 0)
    {
        status.removedUnits = squad->getUnits();
        return;
    }

    // Otherwise reserve enough units to adequately defend the base
    int ourValue = 0;
    for (auto &unit : squad->getUnits())
    {
        ourValue += CombatSim::unitValue(unit);
    }

    int requestedUnits = 0;
    while (ourValue < (enemyValue * 6) / 5)
    {
        requestedUnits++;
        ourValue += CombatSim::unitValue(BWAPI::UnitTypes::Protoss_Zealot);
    }

    // TODO: Request zealot or dragoon when we have that capability
    if (requestedUnits > 0)
    {
        status.unitRequirements.emplace_back(requestedUnits, BWAPI::UnitTypes::Protoss_Dragoon, base->getPosition());
    }
}

void DefendBase::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Always ensure the pylon is built
    if (!pylon && pylonLocation.isValid() && !Builder::pendingHere(pylonLocation))
    {
        auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(pylonLocation), 0, 0, 0);
        prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                   BWAPI::UnitTypes::Protoss_Pylon,
                                                                   buildLocation);
    }

    // Build cannons if necessary
    int neededCannons = desiredCannons() - cannons.size();
    if (neededCannons > 0 && !cannonLocations.empty())
    {
        auto location = *cannonLocations.begin();
        if (!Builder::pendingHere(location))
        {
            auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(*cannonLocations.begin()), 0, 0, 0);
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                                                       buildLocation);
        }
    }
}

int DefendBase::desiredCannons()
{
    // Desire no cannons if the pylon is not yet complete
    if (!pylon || !pylon->completed) return 0;

    // TODO: This should be more nuanced

    // Our main base only needs cannons if the enemy has air-to-ground units
    if (base == Map::getMyMain())
    {
        bool enemyHasAirUnits = false;
        for (auto &unit : Units::allEnemy())
        {
            if (unit->isFlying && unit->canAttackGround())
            {
                enemyHasAirUnits = true;
                break;
            }
        }

        return enemyHasAirUnits ? 2 : 0;
    }

    // Otherwise build cannons only when the enemy is contained
    return Strategist::isEnemyContained() ? 0 : 2;
}