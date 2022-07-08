#include "PathFinding.h"

#include "Map.h"

namespace PathFinding
{
    namespace
    {
        // Define some positions for use in searching outwards from a point at tile resolution
        const BWAPI::Position tileSpiral[] = {
                BWAPI::Position(-32, 0),
                BWAPI::Position(0, -32),
                BWAPI::Position(32, 0),
                BWAPI::Position(0, -32),
                BWAPI::Position(-32, -32),
                BWAPI::Position(32, -32),
                BWAPI::Position(32, 32),
                BWAPI::Position(-32, 32),
                BWAPI::Position(-64, 0),
                BWAPI::Position(0, -64),
                BWAPI::Position(64, 0),
                BWAPI::Position(0, -64),
                BWAPI::Position(-64, -32),
                BWAPI::Position(-32, -64),
                BWAPI::Position(32, -64),
                BWAPI::Position(64, -32),
                BWAPI::Position(64, 32),
                BWAPI::Position(32, 64),
                BWAPI::Position(-32, 64),
                BWAPI::Position(-64, 32)};

        inline bool validChoke(const BWEM::ChokePoint *choke, int minChokeWidth, bool allowMineralWalk)
        {
            const auto &chokeData = Map::choke(choke);
            if (chokeData->width < minChokeWidth) return false;
            if (allowMineralWalk && chokeData->requiresMineralWalk) return true;
            return !choke->Blocked() && !chokeData->requiresMineralWalk;
        }

        bool useNearestBWEMArea(PathFindingOptions options)
        {
            return ((int) options & (int) PathFindingOptions::UseNearestBWEMArea) != 0;
        }

        bool useNeighbouringBWEMArea(PathFindingOptions options)
        {
            return ((int) options & (int) PathFindingOptions::UseNeighbouringBWEMArea) != 0;
        }

        // Adjusts the position so it can be used for BWEM pathfinding given the options
        BWAPI::Position adjustForBWEMPathFinding(BWAPI::Position position, PathFindingOptions options)
        {
            // If we are allowing using the nearest area, accept the input position
            if (useNearestBWEMArea(options)) return position;

            // If the input position has an area, accept it
            auto wp = BWAPI::WalkPosition(position);
            if (BWEM::Map::Instance().GetArea(wp)) return position;

            // If we want to use a neighbour, try to find one and adjust the position
            if (useNeighbouringBWEMArea(options))
            {
                for (const auto &offset : tileSpiral)
                {
                    auto here = position + offset;
                    if (!here.isValid()) continue;
                    if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(here))) return here;
                }
            }

            return BWAPI::Positions::Invalid;
        }

        // Creates a BWEM-style choke point path using an algorithm similar to BWEB's tile-resolution path finding.
        // Used when we want to generate paths with additional constraints beyond what BWEM provides, like taking
        // choke width and mineral walking into consideration.
        BWEM::CPPath CustomChokePointPath(
                BWAPI::Position start,
                BWAPI::Position end,
                PathFindingOptions options,
                BWAPI::UnitType unitType,
                int *pathLength)
        {
            if (pathLength) *pathLength = -1;

            const BWEM::Area *startArea = useNearestBWEMArea(options) ? BWEM::Map::Instance().GetNearestArea(BWAPI::WalkPosition(start))
                                                                      : BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(start));
            const BWEM::Area *targetArea = useNearestBWEMArea(options) ? BWEM::Map::Instance().GetNearestArea(BWAPI::WalkPosition(end))
                                                                       : BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(end));
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
                    if (pathLength) *pathLength = current.dist + end.getApproxDistance(BWAPI::Position(current.choke->Center()));
                    return createPath(current, parentMap);
                }

                // Add valid connected chokes we haven't visited yet
                for (auto choke : current.toArea->ChokePoints())
                {
                    if (validChoke(choke, unitType.width(), unitType.isWorker()) && parentMap.find(choke) == parentMap.end())
                        nodeQueue.emplace(
                                choke,
                                current.dist + BWAPI::Position(choke->Center()).getApproxDistance(BWAPI::Position(current.choke->Center())),
                                chokeTo(choke, current.toArea),
                                current.choke);
                }
            }

            return {};
        }
    }

    int GetGroundDistance(BWAPI::Position start, BWAPI::Position end, BWAPI::UnitType unitType, PathFindingOptions options)
    {
        // Adjust the start and end positions based on the options
        auto adjustedStart = adjustForBWEMPathFinding(start, options);
        auto adjustedEnd = adjustForBWEMPathFinding(end, options);
        if (adjustedStart == BWAPI::Positions::Invalid || adjustedEnd == BWAPI::Positions::Invalid)
        {
            return start.getApproxDistance(end);
        }

        int dist;
        GetChokePointPath(start, end, unitType, options, &dist);
        return dist;
    }

    BWEM::CPPath GetChokePointPath(
            BWAPI::Position start,
            BWAPI::Position end,
            BWAPI::UnitType unitType,
            PathFindingOptions options,
            int *pathLength)
    {
        if (pathLength) *pathLength = -1;

        // Adjust the start and end positions based on the options
        auto adjustedStart = adjustForBWEMPathFinding(start, options);
        auto adjustedEnd = adjustForBWEMPathFinding(end, options);
        if (adjustedStart == BWAPI::Positions::Invalid || adjustedEnd == BWAPI::Positions::Invalid) return BWEM::CPPath();

        // Start with the BWEM path
        auto &bwemPath = BWEM::Map::Instance().GetPath(adjustedStart, adjustedEnd, pathLength);

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
        return CustomChokePointPath(adjustedStart, adjustedEnd, options, unitType, pathLength);
    }

    Choke *SeparatingNarrowChoke(
            BWAPI::Position start,
            BWAPI::Position end,
            BWAPI::UnitType unitType,
            PathFindingOptions options)
    {
        for (auto &bwemChoke : PathFinding::GetChokePointPath(start, end, unitType, options))
        {
            auto thisChoke = Map::choke(bwemChoke);
            if (thisChoke->isNarrowChoke) return thisChoke;
        }

        return nullptr;
    }

    int ExpectedTravelTime(BWAPI::Position start,
                           BWAPI::Position end,
                           BWAPI::UnitType unitType,
                           PathFindingOptions options,
                           double penaltyFactor,
                           int defaultIfInaccessible)
    {
        if (unitType.topSpeed() < 0.0001) return 0;

        if (unitType.isFlyer())
        {
            return (int) ((double) start.getApproxDistance(end) / unitType.topSpeed());
        }

        int dist = GetGroundDistance(start, end, unitType, options);
        if (dist == -1) return defaultIfInaccessible;
        return (int) ((double) dist * penaltyFactor / unitType.topSpeed());
    }
}
