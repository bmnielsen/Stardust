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
        PathNode(BWAPI::TilePosition tilePos, int span, int estimatedCost)
                : tilePos(tilePos)
                , span(span)
                , estimatedCost(estimatedCost)
        {
            ident = nextID++;
        }

        BWAPI::TilePosition tilePos;
        int span;
        int estimatedCost;
        int ident;
    };

    struct PathNodeComparator
    {
        bool operator()(PathNode &a, PathNode &b) const
        {
            return a.estimatedCost > b.estimatedCost || (a.estimatedCost == b.estimatedCost && a.ident < b.ident);
        }
    };
}

namespace Pathfinder
{
    void initializeSearch()
    {
        parents.clear();
        parents.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
    }

    std::vector<BWAPI::TilePosition> Search(BWAPI::TilePosition start,
                                            BWAPI::TilePosition end,
                                            const std::function<bool(const BWAPI::TilePosition &)> &tileValidator,
                                            const std::function<bool(const BWAPI::TilePosition &)> &closeEnoughToEnd,
                                            int maxBacktracking)
    {
#if OUTPUT_SEARCH_TIMING
        auto startTime = std::chrono::high_resolution_clock::now();
        int count = 0;
        auto outputTiming = [&]()
        {
            auto now = std::chrono::high_resolution_clock::now();
            AppLog::Get() << "Path from " << start << " to " << end << "; visited " << count << " node(s) in "
                       << std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count() << "us";
        };
#endif

        /* Estimated distance using diagonal moves */
        auto distToEnd = [&end](BWAPI::TilePosition tilePos)
        {
            int diff1 = std::abs(tilePos.x - end.x);
            int diff2 = std::abs(tilePos.y - end.y);
            if (diff1 > diff2) std::swap(diff1, diff2);
            return diff2 * 10 + diff1 * 4;
        };

        auto tileValid = [&tileValidator](BWAPI::TilePosition tilePos)
        {
            return !tileValidator || tileValidator(tilePos);
        };

        std::priority_queue<PathNode, std::vector<PathNode>, PathNodeComparator> nodeQueue;
        std::fill(parents.begin(), parents.end(), BWAPI::TilePositions::None);

        auto startDist = distToEnd(start);
        auto distCutoff = startDist + maxBacktracking * 10;

        auto visit = [&](PathNode &node, BWAPI::TilePosition direction, bool diagonal = false)
        {
            auto tilePos = node.tilePos + direction;
            if (!tilePos.isValid()) return;
            if (parents[tilePos.x + tilePos.y * BWAPI::Broodwar->mapWidth()] != BWAPI::TilePositions::None) return;
            if (!tileValid(tilePos)) return;

            /* Don't allow diagonal connections between blocked tiles */
            if (diagonal && (!tileValid(BWAPI::TilePosition(tilePos.x, node.tilePos.y)) || !tileValid(BWAPI::TilePosition(node.tilePos.x, tilePos.y))))
            {
                return;
            }

            auto span = distToEnd(tilePos);
            if (span > distCutoff) return;

            int newDist = node.span + (diagonal ? 14 : 10);
            nodeQueue.emplace(tilePos, newDist, newDist + span);
            parents[tilePos.x + tilePos.y * BWAPI::Broodwar->mapWidth()] = node.tilePos;
        };

        nodeQueue.emplace(start, 0, startDist);
        parents[start.x + start.y * BWAPI::Broodwar->mapWidth()] = BWAPI::TilePositions::Invalid;
        while (!nodeQueue.empty())
        {
#if OUTPUT_SEARCH_TIMING
            count++;
#endif

            auto current = nodeQueue.top();
            nodeQueue.pop();

            /* Return path if we are at the destination */
            if (current.tilePos == end || (closeEnoughToEnd && closeEnoughToEnd(current.tilePos)))
            {
                std::vector<BWAPI::TilePosition> outcome;
                BWAPI::TilePosition tilePos = current.tilePos;
                while (tilePos != start)
                {
                    outcome.push_back(tilePos);
                    tilePos = parents[tilePos.x + tilePos.y * BWAPI::Broodwar->mapWidth()];
                }

                std::reverse(outcome.begin(), outcome.end());

#if OUTPUT_SEARCH_TIMING
                outputTiming();
#endif

                return outcome;
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
