#include "PathFinding.h"

#include <BWEB.h>
#include "Map.h"

namespace
{
    auto &bwemMap = BWEM::Map::Instance();
}

namespace PathFinding
{
    namespace
    {
        inline bool validChoke(const BWEM::ChokePoint *choke, int minChokeWidth, bool allowMineralWalk)
        {
            const auto &chokeData = Map::choke(choke);
            if (chokeData->width < minChokeWidth) return false;
            if (allowMineralWalk && chokeData->requiresMineralWalk) return true;
            return !choke->Blocked() && !chokeData->requiresMineralWalk;
        }

        // Creates a BWEM-style choke point path using an algorithm similar to BWEB's tile-resolution path finding.
        // Used when we want to generate paths with additional constraints beyond what BWEM provides, like taking
        // choke width and mineral walking into consideration.
        const BWEM::CPPath CustomChokePointPath(
                BWAPI::Position start,
                BWAPI::Position end,
                bool useNearestBWEMArea,
                BWAPI::UnitType unitType,
                int *pathLength)
        {
            if (pathLength) *pathLength = -1;

            const BWEM::Area *startArea = useNearestBWEMArea ? bwemMap.GetNearestArea(BWAPI::WalkPosition(start))
                                                             : bwemMap.GetArea(BWAPI::WalkPosition(start));
            const BWEM::Area *targetArea = useNearestBWEMArea ? bwemMap.GetNearestArea(BWAPI::WalkPosition(end))
                                                              : bwemMap.GetArea(BWAPI::WalkPosition(end));
            if (!startArea || !targetArea)
            {
                return {};
            }

            if (startArea == targetArea)
            {
                if (pathLength) *pathLength = start.getApproxDistance(end);
                return {};
            }

            struct Node
            {
                Node(const BWEM::ChokePoint *choke, int const dist, const BWEM::Area *toArea, const BWEM::ChokePoint *parent)
                        : choke{choke}, dist{dist}, toArea{toArea}, parent{parent} {}

                mutable const BWEM::ChokePoint *choke;
                mutable int dist;
                mutable const BWEM::Area *toArea;
                mutable const BWEM::ChokePoint *parent = nullptr;
            };

            const auto chokeTo = [](const BWEM::ChokePoint *choke, const BWEM::Area *from)
            {
                return (from == choke->GetAreas().first)
                       ? choke->GetAreas().second
                       : choke->GetAreas().first;
            };

            const auto createPath = [](const Node &node, std::map<const BWEM::ChokePoint *, const BWEM::ChokePoint *> &parentMap)
            {
                std::vector<const BWEM::ChokePoint *> path;
                const BWEM::ChokePoint *current = node.choke;

                while (current)
                {
                    path.push_back(current);
                    current = parentMap[current];
                }

                std::reverse(path.begin(), path.end());

                return path;
            };

            auto cmp = [](Node left, Node right)
            { return left.dist > right.dist; };
            std::priority_queue<Node, std::vector<Node>, decltype(cmp)> nodeQueue(cmp);
            for (auto choke : startArea->ChokePoints())
            {
                if (validChoke(choke, unitType.width(), unitType.isWorker()))
                    nodeQueue.emplace(
                            choke,
                            start.getApproxDistance(BWAPI::Position(choke->Center())),
                            chokeTo(choke, startArea),
                            nullptr);
            }

            std::map<const BWEM::ChokePoint *, const BWEM::ChokePoint *> parentMap;

            while (!nodeQueue.empty())
            {
                auto const current = nodeQueue.top();
                nodeQueue.pop();

                // If already has a parent, continue
                if (parentMap.find(current.choke) != parentMap.end()) continue;

                // Set parent
                parentMap[current.choke] = current.parent;

                // If at target, return path
                // We're ignoring the distance from this last choke to the target position; it's an unlikely
                // edge case that there is an alternate choke giving a significantly better result
                if (current.toArea == targetArea)
                {
                    if (pathLength) *pathLength = current.dist + current.choke->Center().getApproxDistance(BWAPI::WalkPosition(end));
                    return createPath(current, parentMap);
                }

                // Add valid connected chokes we haven't visited yet
                for (auto choke : current.toArea->ChokePoints())
                {
                    if (validChoke(choke, unitType.width(), unitType.isWorker()) && parentMap.find(choke) == parentMap.end())
                        nodeQueue.emplace(
                                choke,
                                current.dist + choke->Center().getApproxDistance(current.choke->Center()),
                                chokeTo(choke, current.toArea),
                                current.choke);
                }
            }

            return {};
        }
    }

    int GetGroundDistance(BWAPI::Position start, BWAPI::Position end, BWAPI::UnitType unitType, PathFindingOptions options)
    {
        // Parse options
        bool useNearestBWEMArea = ((int) options & (int) PathFindingOptions::UseNearestBWEMArea) != 0;

        // If either of the points is not in a BWEM area, fall back to air distance unless the caller overrides this
        if (!useNearestBWEMArea && (!bwemMap.GetArea(BWAPI::WalkPosition(start)) || !bwemMap.GetArea(BWAPI::WalkPosition(end))))
            return start.getApproxDistance(end);

        int dist;
        GetChokePointPath(start, end, unitType, options, &dist);
        return dist;
    }

    const BWEM::CPPath GetChokePointPath(
            BWAPI::Position start,
            BWAPI::Position end,
            BWAPI::UnitType unitType,
            PathFindingOptions options,
            int *pathLength)
    {
        if (pathLength) *pathLength = -1;

        // Parse options
        bool useNearestBWEMArea = ((int) options & (int) PathFindingOptions::UseNearestBWEMArea) != 0;

        // If either of the points is not in a BWEM area, it is probably over unwalkable terrain
        if (!useNearestBWEMArea && (!bwemMap.GetArea(BWAPI::WalkPosition(start)) || !bwemMap.GetArea(BWAPI::WalkPosition(end))))
            return BWEM::CPPath();

        // Start with the BWEM path
        auto bwemPath = bwemMap.GetPath(start, end, pathLength);

        // We can always use BWEM's default pathfinding if:
        // - The minimum choke width is equal to or greater than the unit width
        // - The map doesn't have mineral walking chokes or the unit can't mineral walk
        // An exception to the second case is Plasma, where BWEM doesn't mark the mineral walking chokes as blocked
        bool canUseBwemPath =
                std::max(unitType.width(), unitType.height()) <= Map::minChokeWidth() &&
                Map::mapSpecificOverride()->canUseBwemPath(unitType);

        // If we can't automatically use it, validate the chokes
        if (!canUseBwemPath && !bwemPath.empty())
        {
            canUseBwemPath = true;
            for (auto choke : bwemPath)
            {
                if (!validChoke(choke, unitType.width(), unitType.isWorker()))
                {
                    canUseBwemPath = false;
                    break;
                }
            }
        }

        // Use BWEM path if it is usable
        if (canUseBwemPath)
            return bwemPath;

        // Otherwise do our own path analysis
        return CustomChokePointPath(start, end, useNearestBWEMArea, unitType, pathLength);
    }

    int ExpectedTravelTime(BWAPI::Position start, BWAPI::Position end, BWAPI::UnitType unitType, PathFindingOptions options)
    {
        if (unitType.topSpeed() < 0.0001) return 0;

        int dist = unitType.isFlyer()
                   ? start.getApproxDistance(end)
                   : GetGroundDistance(start, end, unitType, options);
        if (dist <= 0) return 0;
        return (int) ((double) dist * 1.4 / unitType.topSpeed());
    }

    BWAPI::TilePosition NearbyPathfindingTile(BWAPI::TilePosition start)
    {
        for (int radius = 0; radius < 4; radius++)
        {
            for (int x = -radius; x <= radius; x++)
            {
                for (int y = -radius; y <= radius; y++)
                {
                    if (std::abs(x + y) != radius) continue;

                    BWAPI::TilePosition tile = start + BWAPI::TilePosition(x, y);
                    if (!tile.isValid()) continue;
                    if (BWEB::Map::isUsed(tile)) continue;
                    if (!BWEB::Map::isWalkable(tile)) continue;
                    return tile;
                }
            }
        }
        return BWAPI::TilePositions::Invalid;
    }
}
