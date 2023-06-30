#include "PathFinding.h"

#include "Map.h"
#include "Geo.h"

namespace PathFinding
{
    BWAPI::Position NextGridOrChokeWaypoint(BWAPI::Position start, BWAPI::Position end, NavigationGrid *grid, int nodesAhead, bool verifyWalkability)
    {
        // First try to use the grid
        if (grid)
        {
            // Advance the desired number of nodes
            auto node = &(*grid)[start];
            for (int i = 0; node && i < nodesAhead; i++)
            {
                node = node->nextNode;
            }

            if (node)
            {
                auto startTile = BWAPI::TilePosition(start);
                return start + BWAPI::Position(node->tile - startTile);
            }
        }

        // Next try to get a choke-point path
        int length;
        auto path = PathFinding::GetChokePointPath(
                start,
                end,
                BWAPI::UnitTypes::Protoss_Dragoon,
                PathFindingOptions::Default,
                &length);
        if (length == -1) return BWAPI::Positions::Invalid;

        // Find the next waypoint that is at least four tiles away
        BWAPI::Position waypoint = BWAPI::Positions::Invalid;
        for (const auto &bwemChoke : path)
        {
            auto chokeCenter = Map::choke(bwemChoke)->center;
            if (start.getApproxDistance(chokeCenter) > 128)
            {
                waypoint = chokeCenter;
                break;
            }
        }
        if (!waypoint.isValid()) waypoint = end;

        // Compute the desired result
        auto vector = waypoint - start;
        auto result = start + Geo::ScaleVector(vector, nodesAhead * 32);
        if (result == BWAPI::Positions::Invalid) return end; // This means the unit is at the target
        if (!verifyWalkability) return result;

        // Verify that we can walk to this waypoint
        int extent = start.getApproxDistance(result);
        for (int dist = 16; dist < extent; dist += 16)
        {
            auto pos = start + Geo::ScaleVector(vector, dist);
            if (!pos.isValid() || !Map::isWalkable(pos.x >> 5, pos.y >> 5))
            {
                return BWAPI::Positions::Invalid;
            }
        }

        return result;
    }
}