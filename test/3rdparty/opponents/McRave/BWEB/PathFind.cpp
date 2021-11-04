#include "PathFind.h"
#include "BWEB.h"
#include "JPS.h"

using namespace std;
using namespace BWAPI;

namespace BWEB
{
    namespace {

        struct Node {
            TilePosition tile, parent;
            double f, g, h;
            int id = 0;

            Node(TilePosition _tile, TilePosition _parent, double _f, double _g, double _h, int _id) {
                tile = _tile;
                parent = _parent;
                f = _f;
                g = _g;
                h = _h;
                id = _id;
            }

            Node() {
                tile = TilePosition(-1, -1);
            };

            bool operator <(const Node &rhs) const {
                return f > rhs.f;
            }

            bool operator ==(const Node &rhs) const {
                return tile == rhs.tile;
            }
        };

        int oneDim(TilePosition tile) {
            return (tile.y * Broodwar->mapWidth()) + tile.x;
        }

        struct PathCache {
            map<pair<TilePosition, TilePosition>, list<Path>::iterator> iteratorList;
            list<Path> pathCache;
            map<pair<TilePosition, TilePosition>, int> notReachableThisFrame;
        };

        map<function <bool(const TilePosition&)>*, PathCache> pathCache;
        int maxCacheSize = 10000;
        int currentId = 0;
        Node closedSet[65536];
    }

    void Path::generateBFS(function <bool(const TilePosition&)> isWalkable)
    {
        if (!source.isValid() || !target.isValid())
            return;

        // TODO: Add caching

        currentId++;
        if (currentId == INT_MAX)
            currentId = 0;

        queue<Node> openSet;
        vector<TilePosition> direction{ { 0, 1 },{ 1, 0 },{ -1, 0 },{ 0, -1 } };
        openSet.push(Node(target, target, 1.0, 1.0, 1.0, currentId));

        if (diagonal)
            direction.insert(direction.end(), { { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 } });

        // Create the path
        const auto createPath = [&](Node& current) {
            while (current.tile != target) {
                dist += Position(current.tile).getDistance(Position(current.parent));
                tiles.push_back(current.tile);
                current = closedSet[oneDim(current.parent)];
            }
            if (current.tile == target) {
                reachable = true;
                tiles.push_back(target);
            }
        };

        // Iterate the open set
        while (!openSet.empty()) {
            Node parent = openSet.front();
            openSet.pop();
            closedSet[oneDim(parent.tile)] = parent;

            if (parent.tile == source) {
                createPath(parent);
                return;
            }

            for (const auto &d : direction) {
                const auto t = parent.tile + d;

                if (!t.isValid() || !isWalkable(t))
                    continue;

                Node &cs = closedSet[oneDim(t)];

                // Closed Node has been queued or closed
                if (cs.tile != TilePosition(-1, -1) && cs.id == currentId)
                    continue;

                // Check diagonal collisions where necessary
                if (d.x != 0 && d.y != 0 && (!isWalkable(t + TilePosition(d.x, 0)) || !isWalkable(t + TilePosition(0, d.y))))
                    continue;

                openSet.push(Node(t, parent.tile, 0, 0, 0, currentId));
                cs.tile = t;
                cs.id = currentId;
            }
        }
    }

    void Path::generateJPS(function <bool(const TilePosition&)> passedWalkable)
    {
        auto &pathPoints = make_pair(source, target);
        auto &thisCached = pathCache[&passedWalkable];

        if (!target.isValid()
            || !source.isValid())
            return;

        // If this path does not exist in cache, remove last path and erase reference
        if (cached) {
            if (thisCached.iteratorList.find(pathPoints) == thisCached.iteratorList.end()) {
                if (thisCached.pathCache.size() == maxCacheSize) {
                    auto last = thisCached.pathCache.back();
                    thisCached.pathCache.pop_back();
                    thisCached.iteratorList.erase(make_pair(last.getSource(), last.getTarget()));
                }
            }

            // If it does exist, set this path as cached version, update reference and push cached path to the front
            else {
                auto &oldPath = thisCached.iteratorList[pathPoints];
                dist = oldPath->getDistance();
                tiles = oldPath->getTiles();
                reachable = oldPath->isReachable();

                thisCached.pathCache.erase(thisCached.iteratorList[pathPoints]);
                thisCached.pathCache.push_front(*this);
                thisCached.iteratorList[pathPoints] = thisCached.pathCache.begin();
                return;
            }
        }

        vector<TilePosition> newJPSPath;
        const auto width = Broodwar->mapWidth();
        const auto height = Broodwar->mapHeight();

        const auto isWalkable = [&](const int x, const int y) {
            const TilePosition tile(x, y);
            if (x > width || y > height || x < 0 || y < 0)
                return false;
            if (tile == source || tile == target)
                return true;
            if (passedWalkable(tile))
                return true;
            return false;
        };

        // If not reachable based on previous paths to this pair
        if (cached) {
            auto checkReachable = thisCached.notReachableThisFrame[make_pair(source, target)];
            if (checkReachable >= Broodwar->getFrameCount() && Broodwar->getFrameCount() > 0) {
                reachable = false;
                dist = DBL_MAX;
                return;
            }
        }

        // If we found a path, store what was found
        if (JPS::findPath(newJPSPath, isWalkable, source.x, source.y, target.x, target.y)) {
            Position current = Position(source);
            tiles.push_back(source);
            for (auto &t : newJPSPath) {
                dist += Position(t).getDistance(current);
                current = Position(t);
                tiles.push_back(t);
            }
            reachable = true;
        }

        // If not found, set destination pair as unreachable for this frame
        else {
            dist = DBL_MAX;
            reachable = false;
            if (cached)
                thisCached.notReachableThisFrame[make_pair(source, target)] = Broodwar->getFrameCount();
        }

        // Update cache
        if (cached) {
            thisCached.pathCache.push_front(*this);
            thisCached.iteratorList[pathPoints] = thisCached.pathCache.begin();
        }
    }

    void Path::generateAS(function <double(const TilePosition&)> passedHeuristic)
    {
        if (!source.isValid() || !target.isValid())
            return;

        // TODO: Add caching

        currentId++;
        if (currentId == INT_MAX)
            currentId = 0;

        priority_queue <Node> openSet;
        vector<TilePosition> direction{ { 0, 1 },{ 1, 0 },{ -1, 0 },{ 0, -1 } };
        openSet.push(Node(target, target, 1.0, 1.0, 1.0, currentId));

        if (diagonal)
            direction.insert(direction.end(), { { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 } });

        const auto createPath = [&](Node& current) {
            while (current.tile != target) {
                tiles.push_back(current.tile);
                current = closedSet[oneDim(current.parent)];
            }
            if (current.tile == target) {
                reachable = true;
                tiles.push_back(target);
            }
        };

        while (!openSet.empty()) {
            Node parent = openSet.top();
            openSet.pop();
            closedSet[oneDim(parent.tile)] = parent;

            if (parent.tile == source) {
                createPath(parent);
                return;
            }

            for (const auto &d : direction) {
                const auto t = parent.tile + d;

                if (!t.isValid())
                    continue;

                auto g = parent.g + passedHeuristic(t);
                auto h = source.getDistance(t) + ((d.x != 0 && d.y != 0) ? 1.414 : 1.0);
                auto f = g + h;
                Node &cs = closedSet[oneDim(t)];

                // Closed Node has been queued or closed
                if (cs.tile != TilePosition(-1, -1) && cs.id == currentId)
                    continue;

                openSet.push(Node(t, parent.tile, f, g, h, currentId));
                cs.tile = t;
                cs.id = currentId;
            }
        }
    }

    bool Path::terrainWalkable(const TilePosition &tile)
    {
        if (Map::isWalkable(tile, type))
            return true;
        return false;
    }

    bool Path::unitWalkable(const TilePosition &tile)
    {
        if (Map::isWalkable(tile, type) && Map::isUsed(tile) == UnitTypes::None)
            return true;
        return false;
    }

    namespace Pathfinding {
        void clearCacheFully()
        {
            pathCache.clear();
        }

        void clearCache(function <bool(const TilePosition&)> passedWalkable) {
            pathCache[&passedWalkable].iteratorList.clear();
            pathCache[&passedWalkable].pathCache.clear();
        }

        void testCache() {
            for (auto &[_, cache] : pathCache) {
                for (auto &[tile, frame] : cache.notReachableThisFrame) {
                    if (frame >= Broodwar->getFrameCount() - 10)
                        Broodwar->drawLineMap(Position(tile.first), Position(tile.second), Colors::Red);
                }
            }
        }
    }
}