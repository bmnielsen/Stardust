#pragma once

#include "Common.h"
#include <queue>

class NavigationGrid
{
public:
    struct GridNode
    {
        unsigned short x;
        unsigned short y;
        unsigned short cost;
        GridNode *nextNode;
        unsigned char prevNodes;

        GridNode(unsigned short x, unsigned short y) : x(x), y(y), cost(USHRT_MAX), nextNode(nullptr), prevNodes(0) {}

        friend std::ostream &operator<<(std::ostream &os, const GridNode &node)
        {
            os << "(" << node.x << "," << node.y << ":" << node.cost << ")";
            if (node.nextNode) os << "->(" << node.nextNode->x << "," << node.nextNode->y << ":" << node.nextNode->cost << ")";
            return os;
        }

        BWAPI::Position center() const
        {
            return BWAPI::Position((x << 5U) + 16, (y << 5U) + 16);
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

    explicit NavigationGrid(BWAPI::TilePosition goal, BWAPI::TilePosition goalSize = BWAPI::TilePositions::Invalid);

    GridNode &operator[](BWAPI::Position pos);

    const GridNode &operator[](BWAPI::Position pos) const;

    GridNode &operator[](BWAPI::WalkPosition pos);

    const GridNode &operator[](BWAPI::WalkPosition pos) const;

    GridNode &operator[](BWAPI::TilePosition pos);

    const GridNode &operator[](BWAPI::TilePosition pos) const;

    void update();

    void addBlockingObject(BWAPI::TilePosition tile, BWAPI::TilePosition size);

    void removeBlockingObject(BWAPI::TilePosition tile, BWAPI::TilePosition size);

private:
    std::vector<GridNode> grid;
    std::priority_queue<QueueItem, std::vector<QueueItem>, QueueItemComparator> nodeQueue;

    void dumpHeatmap();
};
