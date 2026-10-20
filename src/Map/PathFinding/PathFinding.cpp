#include "PathFinding.h"

#include "Map.h"
#include "Geo.h"

namespace Pathfinder
{
    BWAPI::Position NextGridOrChokeWaypoint(BWAPI::Position start, BWAPI::Position end, NavGrid *grid, int nodesAhead, bool verifyWalkability)
    {
        /* First try to use the grid */
        if (grid)
        {
            /* Advance the desired number of nodes */
            auto node = &(*grid)[start];
            for (int i = 0; node && i < nodesAhead; i++)
            {
                node = node->nextNode;
            }

            if (node)
            {
                auto startTile = BWAPI::TilePosition(start);
                return start + BWAPI::Position(node->tilePos - startTile);
            }
        }

        /* Next try to get a choke-point path */
        int length;
        auto path = Pathfinder::GetChokePointPath(
                start,
                end,
                BWAPI::UnitTypes::Protoss_Dragoon,
                PathFindingOptions::Default,
                &length);
        if (length == -1) return BWAPI::Positions::Invalid;

        /* Find the next waypoint that is at least four tiles away */
        BWAPI::Position waypoint = BWAPI::Positions::Invalid;
        for (const auto &bwemChoke : path)
        {
            auto chokeCenter = Map::choke(bwemChoke)->midpoint;
            if (start.getApproxDistance(chokeCenter) > 128)
            {
                waypoint = chokeCenter;
                break;
            }
        }
        if (!waypoint.isValid()) waypoint = end;

        /* Compute the desired result */
        auto vector = waypoint - start;
        auto outcome = start + Geom::ScaleVector(vector, nodesAhead * 32);
        if (outcome == BWAPI::Positions::Invalid) return end; // This means the unit is at the target
        if (!verifyWalkability) return outcome;

        /* Verify that we can walk to this waypoint */
        int extent = start.getApproxDistance(outcome);
        for (int span = 16; span < extent; span += 16)
        {
            auto pt = start + Geom::ScaleVector(vector, span);
            if (!pt.isValid() || !Map::isWalkable(pt.x >> 5, pt.y >> 5))
            {
                return BWAPI::Positions::Invalid;
            }
        }

        return outcome;
    }
}