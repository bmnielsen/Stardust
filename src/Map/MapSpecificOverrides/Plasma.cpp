#include "Plasma.h"

#include "Map.h"
#include "Geo.h"
#include "Players.h"
#include "UnitUtil.h"
#include "StrategyEngines/MapSpecific/PlasmaStrategyEngine.h"

#include "DebugFlag_UnitOrders.h"

namespace
{
    BWAPI::Position getStartPosition(BWAPI::Unit patch, BWAPI::Unit otherPatch)
    {
        BWAPI::Position bestPos = BWAPI::Positions::Invalid;
        int bestDist = INT_MAX;
        int radius = BWAPI::UnitTypes::Protoss_Probe.sightRange() / 32;
        for (int y = -radius; y <= radius; y++)
        {
            for (int x = -radius; x <= radius; x++)
            {
                BWAPI::TilePosition tile = patch->getInitialTilePosition() + BWAPI::TilePosition(x, y);
                if (!tile.isValid()) continue;
                if (!Map::isWalkable(tile)) continue;

                BWAPI::Position pos = BWAPI::Position(tile) + BWAPI::Position(16, 16);
                int dist = pos.getApproxDistance(otherPatch->getInitialPosition());
                if (dist < bestDist)
                {
                    bestPos = pos;
                    bestDist = dist;
                }
            }
        }

        return bestPos;
    }
}

void Plasma::initializeChokes(std::map<const BWEM::ChokePoint *, Choke *> &chokes)
{
    for (auto &pair : chokes)
    {
        Choke &choke = *pair.second;

        std::set<BWAPI::TilePosition> eggPositions;
        if (BWAPI::TilePosition(choke.center) == BWAPI::TilePosition(70, 15))
        {
            choke.center = BWAPI::Position(BWAPI::WalkPosition(289, 66));
            eggPositions.insert(BWAPI::TilePosition(70, 16));
            eggPositions.insert(BWAPI::TilePosition(71, 16));
            eggPositions.insert(BWAPI::TilePosition(72, 16));
            eggPositions.insert(BWAPI::TilePosition(73, 16));
            eggPositions.insert(BWAPI::TilePosition(74, 16));
            eggPositions.insert(BWAPI::TilePosition(70, 17));
            eggPositions.insert(BWAPI::TilePosition(71, 17));
            eggPositions.insert(BWAPI::TilePosition(72, 17));
            eggPositions.insert(BWAPI::TilePosition(73, 17));
            eggPositions.insert(BWAPI::TilePosition(74, 17));
        }
        else if (BWAPI::TilePosition(choke.center) == BWAPI::TilePosition(11, 58) ||
                 BWAPI::TilePosition(choke.center) == BWAPI::TilePosition(11, 69))
        {
            choke.center = BWAPI::Position(BWAPI::WalkPosition(44, 258));
            eggPositions.insert(BWAPI::TilePosition(9, 62));
            eggPositions.insert(BWAPI::TilePosition(9, 63));
            eggPositions.insert(BWAPI::TilePosition(9, 64));
            eggPositions.insert(BWAPI::TilePosition(9, 65));
            eggPositions.insert(BWAPI::TilePosition(9, 66));
            eggPositions.insert(BWAPI::TilePosition(10, 62));
            eggPositions.insert(BWAPI::TilePosition(10, 63));
            eggPositions.insert(BWAPI::TilePosition(10, 64));
            eggPositions.insert(BWAPI::TilePosition(10, 65));
            eggPositions.insert(BWAPI::TilePosition(10, 66));
        }
        else if (BWAPI::TilePosition(choke.center) == BWAPI::TilePosition(36, 28))
        {
            eggPositions.insert(BWAPI::TilePosition(34, 30));
            eggPositions.insert(BWAPI::TilePosition(35, 30));
            eggPositions.insert(BWAPI::TilePosition(36, 30));
            eggPositions.insert(BWAPI::TilePosition(34, 29));
            eggPositions.insert(BWAPI::TilePosition(35, 29));
            eggPositions.insert(BWAPI::TilePosition(36, 29));
        }
        else if (BWAPI::TilePosition(choke.center) == BWAPI::TilePosition(73, 111) ||
                 BWAPI::TilePosition(choke.center) == BWAPI::TilePosition(70, 113))
        {
            eggPositions.insert(BWAPI::TilePosition(70, 111));
            eggPositions.insert(BWAPI::TilePosition(71, 111));
            eggPositions.insert(BWAPI::TilePosition(72, 111));
            eggPositions.insert(BWAPI::TilePosition(73, 111));
            eggPositions.insert(BWAPI::TilePosition(74, 111));
            eggPositions.insert(BWAPI::TilePosition(70, 112));
            eggPositions.insert(BWAPI::TilePosition(71, 112));
            eggPositions.insert(BWAPI::TilePosition(72, 112));
            eggPositions.insert(BWAPI::TilePosition(73, 112));
            eggPositions.insert(BWAPI::TilePosition(74, 112));
        }
        else if (BWAPI::TilePosition(choke.center) == BWAPI::TilePosition(37, 99) ||
                 BWAPI::TilePosition(choke.center) == BWAPI::TilePosition(36, 99))
        {
            eggPositions.insert(BWAPI::TilePosition(36, 98));
            eggPositions.insert(BWAPI::TilePosition(36, 99));
            eggPositions.insert(BWAPI::TilePosition(36, 100));
            eggPositions.insert(BWAPI::TilePosition(37, 98));
            eggPositions.insert(BWAPI::TilePosition(37, 99));
            eggPositions.insert(BWAPI::TilePosition(37, 100));
        }
        else if (BWAPI::TilePosition(choke.center) == BWAPI::TilePosition(54, 64) ||
                 BWAPI::TilePosition(choke.center) == BWAPI::TilePosition(53, 64))
        {
            eggPositions.insert(BWAPI::TilePosition(52, 65));
            eggPositions.insert(BWAPI::TilePosition(53, 65));
            eggPositions.insert(BWAPI::TilePosition(54, 65));
            eggPositions.insert(BWAPI::TilePosition(52, 66));
            eggPositions.insert(BWAPI::TilePosition(53, 66));
            eggPositions.insert(BWAPI::TilePosition(54, 66));
        }
        else
        {
            continue;
        }

        // Determine if the choke is blocked by eggs, and grab the close mineral patches
        BWAPI::Unit closestMineralPatch = nullptr;
        BWAPI::Unit secondClosestMineralPatch = nullptr;
        int closestMineralPatchDist = INT_MAX;
        int secondClosestMineralPatchDist = INT_MAX;
        for (const auto staticNeutral : BWAPI::Broodwar->getStaticNeutralUnits())
        {
            if (staticNeutral->getType() == BWAPI::UnitTypes::Zerg_Egg)
            {
                auto it = eggPositions.find(staticNeutral->getInitialTilePosition());
                if (it != eggPositions.end())
                {
                    chokeToBlockingEggs[&choke].insert(staticNeutral);
                    eggPositions.erase(it);
                }
            }

            if (staticNeutral->getType() == BWAPI::UnitTypes::Resource_Mineral_Field &&
                staticNeutral->getResources() == 32)
            {
                int dist = staticNeutral->getDistance(choke.center);
                if (dist <= closestMineralPatchDist)
                {
                    secondClosestMineralPatchDist = closestMineralPatchDist;
                    closestMineralPatchDist = dist;
                    secondClosestMineralPatch = closestMineralPatch;
                    closestMineralPatch = staticNeutral;
                }
                else if (dist < secondClosestMineralPatchDist)
                {
                    secondClosestMineralPatchDist = dist;
                    secondClosestMineralPatch = staticNeutral;
                }
            }
        }

        choke.requiresMineralWalk = true;

        auto closestArea = BWEM::Map::Instance().GetNearestArea(
                BWAPI::WalkPosition(closestMineralPatch->getTilePosition()) + BWAPI::WalkPosition(4, 2));
        auto secondClosestArea = BWEM::Map::Instance().GetNearestArea(
                BWAPI::WalkPosition(secondClosestMineralPatch->getTilePosition()) + BWAPI::WalkPosition(4, 2));
        if (closestArea == choke.choke->GetAreas().second &&
            secondClosestArea == choke.choke->GetAreas().first)
        {
            choke.secondAreaMineralPatch = closestMineralPatch;
            choke.firstAreaMineralPatch = secondClosestMineralPatch;
        }
        else
        {
            // Note: Two of the chokes don't have the mineral patches show up in expected areas because of
            // suboptimal BWEM choke placement, but luckily they both follow this pattern
            choke.firstAreaMineralPatch = closestMineralPatch;
            choke.secondAreaMineralPatch = secondClosestMineralPatch;
        }

        choke.firstAreaStartPosition = getStartPosition(choke.firstAreaMineralPatch, choke.secondAreaMineralPatch);
        choke.secondAreaStartPosition = getStartPosition(choke.secondAreaMineralPatch, choke.firstAreaMineralPatch);
    }

    if (chokeToBlockingEggs.size() != 6)
    {
        Log::Get() << "WARNING: Expected to find 6 blocked chokes, but found " << chokeToBlockingEggs.size();
    }
}

void Plasma::onUnitDestroy(BWAPI::Unit unit)
{
    if (unit->getType() != BWAPI::UnitTypes::Zerg_Egg || unit->getPlayer() != BWAPI::Broodwar->neutral()) return;

    for (auto it = chokeToBlockingEggs.begin(); it != chokeToBlockingEggs.end(); it++)
    {
        it->second.erase(unit);

        if (it->second.empty())
        {
            Log::Get() << "Choke @ " << BWAPI::TilePosition(it->first->center) << " unblocked";
            it->first->requiresMineralWalk = false;
            chokeToBlockingEggs.erase(it);
            return;
        }
    }
}

bool Plasma::clusterMove(UnitCluster &cluster, BWAPI::Position targetPosition)
{
    if (!cluster.vanguard) return false;

    // Check if the cluster needs to move through a blocked choke
    Choke *mineralWalkChoke = nullptr;

    // First check if the vanguard unit is close to one of the chokes
    for (const auto &chokeAndBlockingEggs : chokeToBlockingEggs)
    {
        if (cluster.vanguard->getDistance(chokeAndBlockingEggs.first->center) < 200)
        {
            mineralWalkChoke = chokeAndBlockingEggs.first;
            break;
        }
    }

    // Next look for a blocked choke on the path between the cluster's vanguard unit and the target position
    if (!mineralWalkChoke)
    {
        for (const auto &bwemChoke : PathFinding::GetChokePointPath(
                cluster.vanguard->lastPosition,
                targetPosition,
                BWAPI::UnitTypes::Protoss_Probe))
        {
            auto choke = Map::choke(bwemChoke);
            if (choke->requiresMineralWalk)
            {
                mineralWalkChoke = choke;
                break;
            }
        }
        if (!mineralWalkChoke) return false;
    }

    auto it = chokeToBlockingEggs.find(mineralWalkChoke);
    if (it == chokeToBlockingEggs.end()) return false;

    auto &eggs = it->second;

    // Attack with each unit
    for (const auto &myUnit : cluster.units)
    {
        // If the unit is stuck, unstick it
        if (myUnit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!myUnit->isReady()) continue;

        // Attack the closest egg
        int bestDist = INT_MAX;
        BWAPI::Unit bestEgg = nullptr;
        for (const auto &egg : eggs)
        {
            int dist = Geo::EdgeToEdgeDistance(myUnit->type, myUnit->lastPosition, egg->getType(), egg->getInitialPosition());
            if (dist < bestDist)
            {
                bestDist = dist;
                bestEgg = egg;
            }
        }
        if (!bestEgg) continue;

        if (!UnitUtil::IsRangedUnit(myUnit->type))
        {
            if (bestEgg->isVisible())
            {
#if DEBUG_UNIT_ORDERS
                CherryVis::log(myUnit->id) << "Attacking closest egg @ " << BWAPI::WalkPosition(bestEgg->getInitialPosition());
#endif
                myUnit->attack(bestEgg);
            }
            else
            {
#if DEBUG_UNIT_ORDERS
                CherryVis::log(myUnit->id) << "Moving to closest egg @ " << BWAPI::WalkPosition(bestEgg->getInitialPosition());
#endif
                myUnit->moveTo(bestEgg->getInitialPosition(), true);
            }
            continue;
        }

        // Attack if ready
        int weaponRange = myUnit->groundRange();
        if (bestEgg->isVisible() &&
            bestDist <= weaponRange &&
            myUnit->cooldownUntil < (BWAPI::Broodwar->getFrameCount() + BWAPI::Broodwar->getRemainingLatencyFrames() + 2))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(myUnit->id) << "Attacking closest egg @ " << BWAPI::WalkPosition(bestEgg->getInitialPosition());
#endif
            myUnit->attack(bestEgg);
            continue;
        }

        // Otherwise move towards the egg
        // For some reason just moving towards the egg position makes normal BW pathing bug out
        // So we look for the closest position between here and the egg that is walkable and not occupied by a friendly unit
        auto &grid = Players::grid(BWAPI::Broodwar->self());
        int eggHeight = BWAPI::Broodwar->getGroundHeight(bestEgg->getInitialTilePosition());
        int dist;
        for (dist = 64; dist < (bestDist + 16); dist += 16)
        {
            auto vector = Geo::ScaleVector(myUnit->lastPosition - bestEgg->getInitialPosition(), dist);
            if (vector == BWAPI::Positions::Invalid) continue;

            auto pos = bestEgg->getInitialPosition() + vector;
            BWAPI::TilePosition tile(pos);
            if (BWAPI::Broodwar->getGroundHeight(tile) != eggHeight) continue;
            if (!Map::isWalkable(tile)) continue;
            if (grid.collision(pos) > 0) continue;

#if DEBUG_UNIT_ORDERS
            CherryVis::log(myUnit->id) << "Moving towards closest egg @ " << BWAPI::WalkPosition(bestEgg->getInitialPosition())
                                       << "; closest position " << BWAPI::WalkPosition(pos);
#endif

            myUnit->moveTo(pos, true);
            break;
        }
        if (dist >= (bestDist + 16))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(myUnit->id) << "Moving to closest egg @ " << BWAPI::WalkPosition(bestEgg->getInitialPosition());
#endif
            myUnit->moveTo(bestEgg->getInitialPosition(), true);
        }
    }

    return true;
}

void Plasma::addMainBaseBuildingPlacementAreas(std::set<const BWEM::Area *> &areas)
{
    // On Plasma there is very little room in the small main base platform, so use the entire accessible area
    if (accessibleAreas.empty())
    {
        auto mainPos = Map::getMyMain()->getPosition();
        for (const auto &area : BWEM::Map::Instance().Areas())
        {
            if (PathFinding::GetGroundDistance(mainPos, BWAPI::Position(area.Top())) == -1) continue;

            accessibleAreas.push_back(&area);
        }
    }
    areas.insert(accessibleAreas.begin(), accessibleAreas.end());
}

std::unique_ptr<StrategyEngine> Plasma::createStrategyEngine()
{
    return std::make_unique<PlasmaStrategyEngine>();
}
