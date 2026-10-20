#pragma once

#include "Common.h"
#include <queue>

namespace NavigationGridGlobals
{
    void initialize();
}

class NavGrid
{
public:
    struct GridNode
    {
        unsigned short x;
        unsigned short y;
        unsigned short cost;
        GridNode *nextNode;
        unsigned char prevNodes;
        BWAPI::TilePosition tilePos;

        GridNode(unsigned short x, unsigned short y)
            : x(x)
            , y(y)
            , cost(USHRT_MAX)
            , nextNode(nullptr)
            , prevNodes(0)
            , tilePos(BWAPI::TilePosition(x, y)) {}

        friend std::ostream &operator<<(std::ostream &os, const GridNode &node)
        {
            os << "(" << node.x << "," << node.y << ":" << node.cost << ")";
            if (node.nextNode) os << "->(" << node.nextNode->x << "," << node.nextNode->y << ":" << node.nextNode->cost << ")";
            return os;
        }

        [[nodiscard]] BWAPI::Position midpoint() const
        {
            return {(x << 5U) + 16, (y << 5U) + 16};
        }
    };

    typedef std::tuple<unsigned short, GridNode *, bool> QueueItem;

    struct QueueItemComparator
    {
        bool operator()(QueueItem &a, QueueItem &b) const
        {
            return std::get<0>(a) > std::get<0>(b);
        }
    };

    BWAPI::TilePosition goal;

    explicit NavGrid(BWAPI::TilePosition goal, BWAPI::TilePosition goalSize = BWAPI::TilePositions::Invalid);

    GridNode &operator[](BWAPI::Position pt);

    const GridNode &operator[](BWAPI::Position pt) const;

    GridNode &operator[](BWAPI::WalkPosition pt);

    const GridNode &operator[](BWAPI::WalkPosition pt) const;

    GridNode &operator[](BWAPI::TilePosition pt);

    const GridNode &operator[](BWAPI::TilePosition pt) const;

    void update();

    void addBlockingObject(BWAPI::TilePosition tilePos, BWAPI::TilePosition size);

    void addBlockingTiles(const std::set<BWAPI::TilePosition> &tiles);

    void removeBlockingObject(BWAPI::TilePosition tilePos, BWAPI::TilePosition size);

    void removeBlockingTiles(const std::set<BWAPI::TilePosition> &tiles);

private:
    std::vector<GridNode> grid;
    std::priority_queue<QueueItem, std::vector<QueueItem>, QueueItemComparator> nodeQueue;
    std::set<BWAPI::TilePosition> pendingBlockingTiles;

    void updateBlockingTiles();
    void dumpHeatmap();
};
