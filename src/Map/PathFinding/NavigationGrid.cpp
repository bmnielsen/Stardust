#include "NavigationGrid.h"

#include <queue>
#include "Map.h"

#if INSTRUMENTATION_ENABLED
#define NAVIGATION_HEATMAP_ENABLED false
#define OUTPUT_GRID_TIMING false
#endif

#if INSTRUMENTATION_ENABLED_VERBOSE
#define VALIDATE_GRIDS_AFTER_EACH_UPDATE true
#define DEBUG_LOG_UPDATES false
#endif

#define COST_STRAIGHT 32
#define COST_DIAGONAL 45

namespace
{
    int mapWidth;
    int mapHeight;
    
    bool walkableAndNotMineralLine(int x, int y)
    {
        return Map::isWalkable(x, y) && !Map::isInOwnMineralLine(x, y);
    }

    bool allowDiagonalConnectionThrough(int x, int y)
    {
        return Map::isWalkable(x, y) || Map::mapSpecificOverride()->allowDiagonalPathingThrough(x, y);
    }
}

namespace NavigationGridGlobals
{
    void initialize()
    {
        mapWidth = BWAPI::Broodwar->mapWidth();
        mapHeight = BWAPI::Broodwar->mapHeight();
    }
}

NavGrid::NavGrid(BWAPI::TilePosition goal, BWAPI::TilePosition goalSize) : goal(goal)
{
    /* Create each grid cell with its x,y coordinates */
    grid.reserve(mapWidth * mapHeight);
    for (int y = 0; y < mapHeight; y++)
    {
        for (int x = 0; x < mapWidth; x++)
        {
            grid.emplace_back(x, y);
        }
    }

    auto pushInitialTile = [&](BWAPI::TilePosition tilePos)
    {
        if (!tilePos.isValid()) return;

        auto &node = (*this)[tilePos];
        node.cost = 0;
        nodeQueue.emplace(COST_STRAIGHT, &node, false);
        nodeQueue.emplace(COST_DIAGONAL, &node, true);
    };

    if (goalSize.isValid())
    {
        for (int x = -1; x <= goalSize.x; x++)
        {
            pushInitialTile(goal + BWAPI::TilePosition(x, -1));
            pushInitialTile(goal + BWAPI::TilePosition(x, goalSize.y));
        }
        for (int y = 0; y < goalSize.y; y++)
        {
            pushInitialTile(goal + BWAPI::TilePosition(-1, y));
            pushInitialTile(goal + BWAPI::TilePosition(goalSize.x, y));
        }
    }
    else
    {
        pushInitialTile(goal);
    }

    update();
}

NavGrid::GridNode &NavGrid::operator[](BWAPI::Position pt)
{
    return grid[(pt.x >> 5U) + (pt.y >> 5U) * mapWidth];
}

const NavGrid::GridNode &NavGrid::operator[](BWAPI::Position pt) const
{
    return grid[(pt.x >> 5U) + (pt.y >> 5U) * mapWidth];
}

NavGrid::GridNode &NavGrid::operator[](BWAPI::WalkPosition pt)
{
    return grid[(pt.x >> 2U) + (pt.y >> 2U) * mapWidth];
}

const NavGrid::GridNode &NavGrid::operator[](BWAPI::WalkPosition pt) const
{
    return grid[(pt.x >> 2U) + (pt.y >> 2U) * mapWidth];
}

NavGrid::GridNode &NavGrid::operator[](BWAPI::TilePosition pt)
{
    return grid[pt.x + pt.y * mapWidth];
}

const NavGrid::GridNode &NavGrid::operator[](BWAPI::TilePosition pt) const
{
    return grid[pt.x + pt.y * mapWidth];
}

void NavGrid::update()
{
    updateBlockingTiles();
    if (nodeQueue.empty()) return;

#if DEBUG_LOG_UPDATES
    AppLog::Debug() << "Grid-" << goal << ": Updating grid";
#endif

#if OUTPUT_GRID_TIMING
    auto start = std::chrono::high_resolution_clock::now();
#endif

    auto visit = [&](NavGrid::GridNode *current, unsigned short x, unsigned short y, unsigned char direction)
    {
        /* Check validity of the tile */
        /* Since we are using unsigned shorts, we don't need to worry about <0 (they will wrap to be larger than the map) */
        if (x >= mapWidth || y >= mapHeight)
        {
            return;
        }

        auto &node = grid[x + y * mapWidth];

        /* Compute the cost of this node */
        auto cost = current->cost + (direction % 2 == 1 ? COST_DIAGONAL : COST_STRAIGHT);
        /* cost -= std::max((unsigned short)4U, Map::unwalkableProximity(x, y)); */

        /* If the node already has a lower cost, we don't need to consider it */
        if (node.cost <= cost) return;

        /* Don't allow a diagonal connection from a walkable tile through a blocked tile */
        if (direction % 2 == 1 && Map::isWalkable(x, y) &&
            (!allowDiagonalConnectionThrough(x, current->y) || !allowDiagonalConnectionThrough(current->x, y)))
        {
            return;
        }

        /* Make the connection if it isn't already done */
        node.cost = cost;
        if (node.nextNode != current)
        {
            /* Remove the reverse connection if the node was previously connected to another one */
            if (node.nextNode)
            {
                if (node.nextNode->x < node.x)
                {
                    if (node.nextNode->y < node.y)
                    {
                        node.nextNode->prevNodes &= ~(1U << 3U);
                    }
                    else if (node.nextNode->y > node.y)
                    {
                        node.nextNode->prevNodes &= ~(1U << 7U);
                    }
                    else
                    {
                        node.nextNode->prevNodes &= ~(1U << 2U);
                    }
                }
                else if (node.nextNode->x > node.x)
                {
                    if (node.nextNode->y < node.y)
                    {
                        node.nextNode->prevNodes &= ~(1U << 1U);
                    }
                    else if (node.nextNode->y > node.y)
                    {
                        node.nextNode->prevNodes &= ~(1U << 5U);
                    }
                    else
                    {
                        node.nextNode->prevNodes &= ~(1U << 0U);
                    }
                }
                else
                {
                    if (node.nextNode->y < node.y)
                    {
                        node.nextNode->prevNodes &= ~(1U << 6U);
                    }
                    else
                    {
                        node.nextNode->prevNodes &= ~(1U << 4U);
                    }
                }
            }

            /* Create the connection */
            node.nextNode = current;
            current->prevNodes |= 1U << direction;
        }

        /* Queue the node if it is walkable */
        if (walkableAndNotMineralLine(x, y))
        {
#if DEBUG_LOG_UPDATES
            AppLog::Debug() << "Grid-" << goal << ": Queueing " << node;
#endif

            nodeQueue.emplace(node.cost + COST_STRAIGHT, &node, false);
            nodeQueue.emplace(node.cost + COST_DIAGONAL, &node, true);
        }
    };

    while (!nodeQueue.empty())
    {
        std::tuple<unsigned short, GridNode *, bool> current = nodeQueue.top();
        nodeQueue.pop();

        auto node = std::get<1>(current);

        if (!walkableAndNotMineralLine(node->x, node->y) || node->cost == USHRT_MAX) continue;

#if DEBUG_LOG_UPDATES
        AppLog::Debug() << "Grid-" << goal << ": Processing " << *node << "; diag=" << std::get<2>(current);
#endif

        /* Boolean controls whether we are considering diagonal edges or straight edges */
        if (std::get<2>(current))
        {
            /* Diagonals */
            visit(node, node->x - 1, node->y + 1, 1);
            visit(node, node->x + 1, node->y + 1, 3);
            visit(node, node->x - 1, node->y - 1, 5);
            visit(node, node->x + 1, node->y - 1, 7);
        }
        else
        {
            /* Straight edges */
            visit(node, node->x - 1, node->y, 0);
            visit(node, node->x + 1, node->y, 2);
            visit(node, node->x, node->y - 1, 4);
            visit(node, node->x, node->y + 1, 6);
        }
    }

#if OUTPUT_GRID_TIMING
    auto now = std::chrono::high_resolution_clock::now();
    AppLog::Get() << "Update navigation grid " << goal << ": " << std::chrono::duration_cast<std::chrono::microseconds>(now - start).count() << "us";
#endif

#if NAVIGATION_HEATMAP_ENABLED
    dumpHeatmap();
#endif

#if VALIDATE_GRIDS_AFTER_EACH_UPDATE
    for (int y = 0; y < mapHeight; y++)
    {
        for (int x = 0; x < mapWidth; x++)
        {
            auto &node = grid[x + y * mapWidth];

            if (node.cost > 0 && node.cost < USHRT_MAX && !node.nextNode)
            {
                AppLog::Get() << "ERROR: Grid-" << goal << ": Node with no next node " << node;
                return;
            }

            if (node.cost == 0 && node.nextNode)
            {
                AppLog::Get() << "ERROR: Grid-" << goal << ": Goal node with next node " << node;
                return;
            }

            if (node.nextNode)
            {
                int costDiff = node.cost - node.nextNode->cost;
                if (costDiff <= 0)
                {
                    AppLog::Get() << "ERROR: Grid-" << goal << ": Non-decreasing cost from " << node << " to " << (*node.nextNode) << ": " << costDiff;
                }
                if (costDiff != COST_STRAIGHT && costDiff != COST_DIAGONAL)
                {
                    AppLog::Get() << "ERROR: Grid-" << goal << ": Invalid cost from " << node << " to " << (*node.nextNode) << ": " << costDiff;
                    return;
                }
                if (costDiff == COST_DIAGONAL && Map::isWalkable(x, y) &&
                    (!allowDiagonalConnectionThrough(node.nextNode->x, y) || !allowDiagonalConnectionThrough(x, node.nextNode->y)))
                {
                    AppLog::Get() << "ERROR: Grid-" << goal << ": Diagonal path through blocked tile from " << node << " to " << (*node.nextNode);
                    return;
                }
            }

            auto current = node.nextNode;
            int i = 0;
            while (current != nullptr && i < 5)
            {
                if (current == &node)
                {
                    AppLog::Get() << "ERROR: Grid-" << goal << ": Loop between " << node << " and " << *current;
                    return;
                }

                if (!Map::isWalkable(current->x, current->y))
                {
                    AppLog::Get() << "ERROR: Grid-" << goal << ": Path from " << node << " goes through unwalkable tile " << *current;
                    return;
                }

                current = current->nextNode;
                i++;
            }
        }
    }
#endif
}

void NavGrid::addBlockingObject(BWAPI::TilePosition tilePos, BWAPI::TilePosition size)
{
#if DEBUG_LOG_UPDATES
    AppLog::Debug() << "Grid-" << goal << ": addBlockingObject(" << tilePos << "," << size << ")";
#endif

    for (int x = tilePos.x; x < tilePos.x + size.x; x++)
    {
        for (int y = tilePos.y; y < tilePos.y + size.y; y++)
        {
            pendingBlockingTiles.insert(BWAPI::TilePosition(x, y));
        }
    }

    /* Add tiles that have diagonal connections through a corner of the object */
    auto addCornerTile = [&](int toX, int toY, unsigned int direction)
    {
        if (toX < 0) return;
        if (toX >= mapWidth) return;
        if (toY < 0) return;
        if (toY >= mapHeight) return;

        auto &node = grid[toX + toY * mapWidth];
        if (node.prevNodes & (1U << direction))
        {
#if DEBUG_LOG_UPDATES
            AppLog::Debug() << "Grid-" << goal << ": Added corner tile " << BWAPI::TilePosition(toX, toY) << " for direction " << direction;
#endif

            pendingBlockingTiles.insert(BWAPI::TilePosition(toX, toY));
        }
    };
    addCornerTile(tilePos.x, tilePos.y - 1, 1); // up-right, top-left corner
    addCornerTile(tilePos.x + size.x, tilePos.y + size.y - 1, 1); // up-right, bottom-right corner
    addCornerTile(tilePos.x + size.x - 1, tilePos.y - 1, 3); // up-left, top-right corner
    addCornerTile(tilePos.x - 1, tilePos.y + size.y - 1, 3); // up-left, bottom-left corner
    addCornerTile(tilePos.x + size.x, tilePos.y, 5); // down-right, top-right corner
    addCornerTile(tilePos.x, tilePos.y + size.y, 5); // down-right, bottom-left corner
    addCornerTile(tilePos.x - 1, tilePos.y, 7); // down-left, top-left corner
    addCornerTile(tilePos.x + size.x - 1, tilePos.y + size.y, 7); // down-left, bottom-right corner
}

void NavGrid::addBlockingTiles(const std::set<BWAPI::TilePosition> &tiles)
{
    pendingBlockingTiles.insert(tiles.begin(), tiles.end());
}

void NavGrid::removeBlockingObject(BWAPI::TilePosition tilePos, BWAPI::TilePosition size)
{
#if DEBUG_LOG_UPDATES
    AppLog::Debug() << "Grid-" << goal << ": removeBlockingObject(" << tilePos << "," << size << ")";
#endif

    std::set<BWAPI::TilePosition> tiles;
    for (int x = tilePos.x; x < tilePos.x + size.x; x++)
    {
        for (int y = tilePos.y; y < tilePos.y + size.y; y++)
        {
            tiles.insert(BWAPI::TilePosition(x, y));
        }
    }

    removeBlockingTiles(tiles);
}

void NavGrid::removeBlockingTiles(const std::set<BWAPI::TilePosition> &tiles)
{
    /* Reset the cost of all nodes corresponding to the blocking tiles being removed */
    /* While doing this, gather tiles that potentially border these blocking tiles */
    /* Finally add the valid border tiles to the update queue */

    std::set<GridNode *> bordering;
    auto addBordering = [&](unsigned short x, unsigned short y)
    {
        if (x >= mapWidth || y >= mapHeight)
        {
            return;
        }

        bordering.insert(&grid[x + y * mapWidth]);
    };

    auto visit = [&](unsigned short x, unsigned short y)
    {
        auto &node = grid[x + y * mapWidth];

        node.cost = USHRT_MAX;
        node.nextNode = nullptr;

        addBordering(x + 1, y);
        addBordering(x - 1, y);
        addBordering(x, y + 1);
        addBordering(x, y - 1);
        addBordering(x - 1, y - 1);
        addBordering(x + 1, y - 1);
        addBordering(x - 1, y + 1);
        addBordering(x + 1, y + 1);
    };
    for (const auto &tilePos : tiles)
    {
        visit(tilePos.x, tilePos.y);
    }

    for (auto node : bordering)
    {
        if (node->nextNode)
        {
#if DEBUG_LOG_UPDATES
            AppLog::Debug() << "Grid-" << goal << ": removeBlocking, enqueued " << node->tilePos;
#endif

            nodeQueue.emplace(node->cost + COST_STRAIGHT, node, false);
            nodeQueue.emplace(node->cost + COST_DIAGONAL, node, true);
        }
    }
}

void NavGrid::updateBlockingTiles()
{
    if (pendingBlockingTiles.empty()) return;

#if DEBUG_LOG_UPDATES
    AppLog::Debug() << "Grid-" << goal << ": Updating blocking tiles";
#endif

    /* First, invalidate every path that goes through the tiles */
    /* Then push all still-valid tiles bordering an invalidated tile to the update queue */
    /* When the grid is updated, all of the invalidated tiles will receive a new path from these bordering tiles */

    std::queue<GridNode *> queue;
    for (const auto &tilePos : pendingBlockingTiles)
    {
        auto &node = grid[tilePos.x + tilePos.y * mapWidth];
        queue.push(&node);
    }

    std::set<GridNode *> bordering;
    auto addBordering = [&](unsigned short x, unsigned short y)
    {
        if (x >= mapWidth || y >= mapHeight)
        {
            return;
        }

        bordering.insert(&grid[x + y * mapWidth]);
    };

    auto visit = [&](NavGrid::GridNode *current, unsigned short x, unsigned short y)
    {
        auto &node = grid[x + y * mapWidth];

        node.cost = USHRT_MAX;
        node.nextNode = nullptr;
        if (node.prevNodes) queue.push(&node);

        addBordering(x + 1, y);
        addBordering(x - 1, y);
        addBordering(x, y + 1);
        addBordering(x, y - 1);
        addBordering(x - 1, y - 1);
        addBordering(x + 1, y - 1);
        addBordering(x - 1, y + 1);
        addBordering(x + 1, y + 1);
    };

    while (!queue.empty())
    {
        GridNode *current = queue.front();
        queue.pop();

        if (current->prevNodes & (1U << 1U)) visit(current, current->x - 1, current->y + 1);
        if (current->prevNodes & (1U << 3U)) visit(current, current->x + 1, current->y + 1);
        if (current->prevNodes & (1U << 5U)) visit(current, current->x - 1, current->y - 1);
        if (current->prevNodes & (1U << 7U)) visit(current, current->x + 1, current->y - 1);

        if (current->prevNodes & (1U << 0U)) visit(current, current->x - 1, current->y);
        if (current->prevNodes & (1U << 2U)) visit(current, current->x + 1, current->y);
        if (current->prevNodes & (1U << 4U)) visit(current, current->x, current->y - 1);
        if (current->prevNodes & (1U << 6U)) visit(current, current->x, current->y + 1);

        current->prevNodes = 0;
    }

    /* Push all valid bordering tiles to the update queue */
    for (auto &borderingNode : bordering)
    {
        if (borderingNode->nextNode)
        {
#if DEBUG_LOG_UPDATES
            AppLog::Debug() << "Grid-" << goal << ": updateBlockingTiles, enqueued " << borderingNode->tilePos;
#endif

            nodeQueue.emplace(borderingNode->cost + COST_STRAIGHT, borderingNode, false);
            nodeQueue.emplace(borderingNode->cost + COST_DIAGONAL, borderingNode, true);
        }
    }

    pendingBlockingTiles.clear();
}

void NavGrid::dumpHeatmap()
{
    /* Dump to CherryVis */
    std::vector<long> costs(mapWidth * mapHeight);
    for (int y = 0; y < mapHeight; y++)
    {
        for (int x = 0; x < mapWidth; x++)
        {
            auto cost = grid[x + y * mapWidth].cost;
            costs[x + y * mapWidth] = cost == USHRT_MAX ? 0 : cost;
        }
    }

    Telemetry::addHeatmap((std::ostringstream() << "Navigation@" << goal).str(), costs, mapWidth, mapHeight);
}
