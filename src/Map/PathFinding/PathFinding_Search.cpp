#include "PathFinding.h"

#include "Geo.h"

#if INSTRUMENTATION_ENABLED
#define OUTPUT_SEARCH_TIMING false
#endif

namespace
{
    std::vector<BWAPI::TilePosition> parents;
    int nextID;

    struct PathNode
    {
        PathNode(BWAPI::TilePosition tile, int dist, int estimatedCost)
                : tile(tile)
                , dist(dist)
                , estimatedCost(estimatedCost)
        {
            id = nextID++;
        }

        BWAPI::TilePosition tile;
        int dist;
        int estimatedCost;
        int id;
    };

    struct PathNodeComparator
    {
        bool operator()(PathNode &a, PathNode &b) const
        {
            return a.estimatedCost > b.estimatedCost || (a.estimatedCost == b.estimatedCost && a.id < b.id);
        }
    };
}

namespace PathFinding
{
    void initializeSearch()
    {
        parents.clear();
        parents.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
    }

    std::vector<BWAPI::TilePosition> Search(BWAPI::TilePosition start,
                                            BWAPI::TilePosition end,
                                            const std::function<bool(const BWAPI::TilePosition &)> &tileValidator,
                                            const std::function<bool(const BWAPI::TilePosition &)> &closeEnoughToEnd)
    {
#if OUTPUT_SEARCH_TIMING
        auto startTime = std::chrono::high_resolution_clock::now();
        int count = 0;
        auto outputTiming = [&]()
        {
            auto now = std::chrono::high_resolution_clock::now();
            Log::Get() << "Path from " << start << " to " << end << "; visited " << count << " node(s) in "
                       << std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count() << "us";
        };
#endif

        // Estimated distance using diagonal moves
        auto distToEnd = [&end](BWAPI::TilePosition tile)
        {
            int diff1 = std::abs(tile.x - end.x);
            int diff2 = std::abs(tile.y - end.y);
            if (diff1 > diff2) std::swap(diff1, diff2);
            return diff2 * 10 + diff1 * 4;
        };

        auto tileValid = [&tileValidator](BWAPI::TilePosition tile)
        {
            return !tileValidator || tileValidator(tile);
        };

        std::priority_queue<PathNode, std::vector<PathNode>, PathNodeComparator> nodeQueue;
        std::fill(parents.begin(), parents.end(), BWAPI::TilePositions::None);

        auto visit = [&](PathNode &node, BWAPI::TilePosition direction, bool diagonal = false)
        {
            auto tile = node.tile + direction;
            if (!tile.isValid()) return;
            if (parents[tile.x + tile.y * BWAPI::Broodwar->mapWidth()] != BWAPI::TilePositions::None) return;
            if (!tileValid(tile)) return;

            // Don't allow diagonal connections between blocked tiles
            if (diagonal && (!tileValid(BWAPI::TilePosition(tile.x, node.tile.y)) || !tileValid(BWAPI::TilePosition(node.tile.x, tile.y))))
            {
                return;
            }

            int newDist = node.dist + (diagonal ? 14 : 10);
            nodeQueue.emplace(tile, newDist, newDist + distToEnd(tile));
            parents[tile.x + tile.y * BWAPI::Broodwar->mapWidth()] = node.tile;
        };

        nextID = 0;
        nodeQueue.emplace(start, 0, distToEnd(start));
        parents[start.x + start.y * BWAPI::Broodwar->mapWidth()] = BWAPI::TilePositions::Invalid;
        while (!nodeQueue.empty())
        {
#if OUTPUT_SEARCH_TIMING
            count++;
#endif

            auto current = nodeQueue.top();
            nodeQueue.pop();

            // Return path if we are at the destination
            if (current.tile == end || (closeEnoughToEnd && closeEnoughToEnd(current.tile)))
            {
                std::vector<BWAPI::TilePosition> result;
                BWAPI::TilePosition tile = current.tile;
                while (tile != start)
                {
                    result.push_back(tile);
                    tile = parents[tile.x + tile.y * BWAPI::Broodwar->mapWidth()];
                }

                std::reverse(result.begin(), result.end());

#if OUTPUT_SEARCH_TIMING
                outputTiming();
#endif

                return result;
            }

            visit(current, BWAPI::TilePosition(1, 0));
            visit(current, BWAPI::TilePosition(0, 1));
            visit(current, BWAPI::TilePosition(-1, 0));
            visit(current, BWAPI::TilePosition(0, -1));
            visit(current, BWAPI::TilePosition(1, 1), true);
            visit(current, BWAPI::TilePosition(-1, 1), true);
            visit(current, BWAPI::TilePosition(-1, -1), true);
            visit(current, BWAPI::TilePosition(1, -1), true);
        }

#if OUTPUT_SEARCH_TIMING
        outputTiming();
#endif

        return {};
    }
}
