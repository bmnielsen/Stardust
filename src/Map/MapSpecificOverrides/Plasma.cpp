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
                BWAPI::TilePosition tilePos = patch->getInitialTilePosition() + BWAPI::TilePosition(x, y);
                if (!tilePos.isValid()) continue;
                if (!Map::isWalkable(tilePos)) continue;

                BWAPI::Position pt = BWAPI::Position(tilePos) + BWAPI::Position(16, 16);
                int span = pt.getApproxDistance(otherPatch->getInitialPosition());
                if (span < bestDist)
                {
                    bestPos = pt;
                    bestDist = span;
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
        if (BWAPI::TilePosition(choke.midpoint) == BWAPI::TilePosition(70, 15))
        {
            choke.midpoint = BWAPI::Position(BWAPI::WalkPosition(289, 66));
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
        else if (BWAPI::TilePosition(choke.midpoint) == BWAPI::TilePosition(11, 58) ||
                 BWAPI::TilePosition(choke.midpoint) == BWAPI::TilePosition(11, 69))
        {
            choke.midpoint = BWAPI::Position(BWAPI::WalkPosition(44, 258));
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
        else if (BWAPI::TilePosition(choke.midpoint) == BWAPI::TilePosition(36, 28))
        {
            eggPositions.insert(BWAPI::TilePosition(34, 30));
            eggPositions.insert(BWAPI::TilePosition(35, 30));
            eggPositions.insert(BWAPI::TilePosition(36, 30));
            eggPositions.insert(BWAPI::TilePosition(34, 29));
            eggPositions.insert(BWAPI::TilePosition(35, 29));
            eggPositions.insert(BWAPI::TilePosition(36, 29));
        }
        else if (BWAPI::TilePosition(choke.midpoint) == BWAPI::TilePosition(73, 111) ||
                 BWAPI::TilePosition(choke.midpoint) == BWAPI::TilePosition(70, 113))
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
        else if (BWAPI::TilePosition(choke.midpoint) == BWAPI::TilePosition(37, 99) ||
                 BWAPI::TilePosition(choke.midpoint) == BWAPI::TilePosition(36, 99))
        {
            eggPositions.insert(BWAPI::TilePosition(36, 98));
            eggPositions.insert(BWAPI::TilePosition(36, 99));
            eggPositions.insert(BWAPI::TilePosition(36, 100));
            eggPositions.insert(BWAPI::TilePosition(37, 98));
            eggPositions.insert(BWAPI::TilePosition(37, 99));
            eggPositions.insert(BWAPI::TilePosition(37, 100));
        }
        else if (BWAPI::TilePosition(choke.midpoint) == BWAPI::TilePosition(54, 64) ||
                 BWAPI::TilePosition(choke.midpoint) == BWAPI::TilePosition(53, 64))
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

        /* Determine if the choke is blocked by eggs, and grab the close mineral patches */
        BWAPI::Unit closestMineralPatch = nullptr;
        BWAPI::Unit secondClosestMineralPatch = nullptr;
        int closestMineralPatchDist = INT_MAX;
        int secondClosestMineralPatchDist = INT_MAX;
        for (const auto staticNeutral : BWAPI::Broodwar->getStaticNeutralUnits())
        {
            if (staticNeutral->getType() == BWAPI::UnitTypes::Zerg_Egg)
            {
                auto iterRef = eggPositions.find(staticNeutral->getInitialTilePosition());
                if (iterRef != eggPositions.end())
                {
                    chokeToBlockingEggs[&choke].insert(staticNeutral);
                    eggPositions.erase(iterRef);
                }
            }

            if (staticNeutral->getType() == BWAPI::UnitTypes::Resource_Mineral_Field &&
                staticNeutral->getResources() == 32)
            {
                int span = staticNeutral->getDistance(choke.midpoint);
                if (span <= closestMineralPatchDist)
                {
                    secondClosestMineralPatchDist = closestMineralPatchDist;
                    closestMineralPatchDist = span;
                    secondClosestMineralPatch = closestMineralPatch;
                    closestMineralPatch = staticNeutral;
                }
                else if (span < secondClosestMineralPatchDist)
                {
                    secondClosestMineralPatchDist = span;
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
            /* Note: Two of the chokes don't have the mineral patches show up in expected areas because of */
            /* suboptimal BWEM choke placement, but luckily they both follow this pattern */
            choke.firstAreaMineralPatch = closestMineralPatch;
            choke.secondAreaMineralPatch = secondClosestMineralPatch;
        }

        choke.firstAreaStartPosition = getStartPosition(choke.firstAreaMineralPatch, choke.secondAreaMineralPatch);
        choke.secondAreaStartPosition = getStartPosition(choke.secondAreaMineralPatch, choke.firstAreaMineralPatch);
    }

    if (chokeToBlockingEggs.size() != 6)
    {
        AppLog::Get() << "WARNING: Expected to find 6 blocked chokes, but found " << chokeToBlockingEggs.size();
    }
}

void Plasma::onUnitDestroy(BWAPI::Unit gameUnit)
{
    if (gameUnit->getType() != BWAPI::UnitTypes::Zerg_Egg || gameUnit->getPlayer() != BWAPI::Broodwar->neutral()) return;

    for (auto iterRef = chokeToBlockingEggs.begin(); iterRef != chokeToBlockingEggs.end(); iterRef++)
    {
        iterRef->second.erase(gameUnit);

        if (iterRef->second.empty())
        {
            AppLog::Get() << "Choke @ " << BWAPI::TilePosition(iterRef->first->midpoint) << " unblocked";
            iterRef->first->requiresMineralWalk = false;
            chokeToBlockingEggs.erase(iterRef);
            return;
        }
    }
}

bool Plasma::clusterMove(UnitCluster &clump, BWAPI::Position targetPosition)
{
    if (!clump.vanguard) return false;

    /* Check if the cluster needs to move through a blocked choke */
    Choke *mineralWalkChoke = nullptr;

    /* First check if the vanguard unit is close to one of the chokes */
    for (const auto &chokeAndBlockingEggs : chokeToBlockingEggs)
    {
        if (clump.vanguard->getDistance(chokeAndBlockingEggs.first->midpoint) < 200)
        {
            mineralWalkChoke = chokeAndBlockingEggs.first;
            break;
        }
    }

    /* Next look for a blocked choke on the path between the cluster's vanguard unit and the target position */
    if (!mineralWalkChoke)
    {
        for (const auto &bwemChoke : Pathfinder::GetChokePointPath(
                clump.vanguard->prevPos,
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

    auto iterRef = chokeToBlockingEggs.find(mineralWalkChoke);
    if (iterRef == chokeToBlockingEggs.end()) return false;

    auto &eggs = iterRef->second;

    /* Attack with each unit */
    for (const auto &myUnit : clump.units)
    {
        /* If the unit is stuck, unstick it */
        if (myUnit->unstick()) continue;

        /* If the unit is not ready (i.e. is already in the middle of an attack), don't touch it */
        if (!myUnit->isReady()) continue;

        /* Attack the closest egg */
        int bestDist = INT_MAX;
        BWAPI::Unit bestEgg = nullptr;
        for (const auto &egg : eggs)
        {
            int span = Geom::EdgeToEdgeDistance(myUnit->type, myUnit->prevPos, egg->getType(), egg->getInitialPosition());
            if (span < bestDist)
            {
                bestDist = span;
                bestEgg = egg;
            }
        }
        if (!bestEgg) continue;

        if (!UnitHelper::IsRangedUnit(myUnit->type))
        {
            if (bestEgg->isVisible())
            {
#if DEBUG_UNIT_ORDERS
                Telemetry::log(myUnit->ident) << "Attacking closest egg @ " << BWAPI::WalkPosition(bestEgg->getInitialPosition());
#endif
                myUnit->attack(bestEgg);
            }
            else
            {
#if DEBUG_UNIT_ORDERS
                Telemetry::log(myUnit->ident) << "Moving to closest egg @ " << BWAPI::WalkPosition(bestEgg->getInitialPosition());
#endif
                myUnit->moveTo(bestEgg->getInitialPosition(), true);
            }
            continue;
        }

        /* Attack if ready */
        int weaponRange = myUnit->groundRange();
        if (bestEgg->isVisible() &&
            bestDist <= weaponRange &&
            myUnit->cooldownUntil < (frameNow + BWAPI::Broodwar->getRemainingLatencyFrames() + 2))
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(myUnit->ident) << "Attacking closest egg @ " << BWAPI::WalkPosition(bestEgg->getInitialPosition());
#endif
            myUnit->attack(bestEgg);
            continue;
        }

        /* Otherwise move towards the egg */
        /* For some reason just moving towards the egg position makes normal BW pathing bug out */
        /* So we look for the closest position between here and the egg that is walkable and not occupied by a friendly unit */
        auto &grid = PlayerInfo::grid(BWAPI::Broodwar->self());
        int eggHeight = BWAPI::Broodwar->getGroundHeight(bestEgg->getInitialTilePosition());
        int span;
        for (span = 64; span < (bestDist + 16); span += 16)
        {
            auto vector = Geom::ScaleVector(myUnit->prevPos - bestEgg->getInitialPosition(), span);
            if (vector == BWAPI::Positions::Invalid) continue;

            auto pt = bestEgg->getInitialPosition() + vector;
            BWAPI::TilePosition tilePos(pt);
            if (BWAPI::Broodwar->getGroundHeight(tilePos) != eggHeight) continue;
            if (!Map::isWalkable(tilePos)) continue;
            if (grid.collision(pt) > 0) continue;

#if DEBUG_UNIT_ORDERS
            Telemetry::log(myUnit->ident) << "Moving towards closest egg @ " << BWAPI::WalkPosition(bestEgg->getInitialPosition())
                                       << "; closest position " << BWAPI::WalkPosition(pt);
#endif

            myUnit->moveTo(pt, true);
            break;
        }
        if (span >= (bestDist + 16))
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(myUnit->ident) << "Moving to closest egg @ " << BWAPI::WalkPosition(bestEgg->getInitialPosition());
#endif
            myUnit->moveTo(bestEgg->getInitialPosition(), true);
        }
    }

    return true;
}

void Plasma::modifyMainBaseBuildingPlacementAreas(std::set<const BWEM::Area *> &areas)
{
    /* On Plasma there is very little room in the small main base platform, so use the entire accessible area */
    if (accessibleAreas.empty())
    {
        auto mainPos = Map::getMyMain()->getPosition();
        for (const auto &area : BWEM::Map::Instance().Areas())
        {
            if (Pathfinder::GetGroundDistance(mainPos, BWAPI::Position(area.Top())) == -1) continue;

            accessibleAreas.push_back(&area);
        }
    }
    areas.insert(accessibleAreas.begin(), accessibleAreas.end());
}

std::unique_ptr<StrategyEngine> Plasma::createStrategyEngine()
{
    return std::make_unique<PlasmaStrategyEngine>();
}
