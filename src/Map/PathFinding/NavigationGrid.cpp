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

NavigationGrid::NavigationGrid(BWAPI::TilePosition goal, BWAPI::TilePosition goalSize) : goal(goal)
{
    // Create each grid cell with its x,y coordinates
    grid.reserve(mapWidth * mapHeight);
    for (int y = 0; y < mapHeight; y++)
    {
        for (int x = 0; x < mapWidth; x++)
        {
            grid.emplace_back(x, y);
        }
    }

    auto pushInitialTile = [&](BWAPI::TilePosition tile)
    {
        if (!tile.isValid()) return;

        auto &node = (*this)[tile];
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

NavigationGrid::GridNode &NavigationGrid::operator[](BWAPI::Position pos)
{
    return grid[(pos.x >> 5U) + (pos.y >> 5U) * mapWidth];
}

const NavigationGrid::GridNode &NavigationGrid::operator[](BWAPI::Position pos) const
{
    return grid[(pos.x >> 5U) + (pos.y >> 5U) * mapWidth];
}

NavigationGrid::GridNode &NavigationGrid::operator[](BWAPI::WalkPosition pos)
{
    return grid[(pos.x >> 2U) + (pos.y >> 2U) * mapWidth];
}

const NavigationGrid::GridNode &NavigationGrid::operator[](BWAPI::WalkPosition pos) const
{
    return grid[(pos.x >> 2U) + (pos.y >> 2U) * mapWidth];
}

NavigationGrid::GridNode &NavigationGrid::operator[](BWAPI::TilePosition pos)
{
    return grid[pos.x + pos.y * mapWidth];
}

const NavigationGrid::GridNode &NavigationGrid::operator[](BWAPI::TilePosition pos) const
{
    return grid[pos.x + pos.y * mapWidth];
}

void NavigationGrid::update()
{
    updateBlockingTiles();
    if (nodeQueue.empty()) return;

#if DEBUG_LOG_UPDATES
    Log::Debug() << "Grid-" << goal << ": Updating grid";
#endif

#if OUTPUT_GRID_TIMING
    auto start = std::chrono::high_resolution_clock::now();
#endif

    auto visit = [&](NavigationGrid::GridNode *current, unsigned short x, unsigned short y, unsigned char direction)
    {
        // Check validity of the tile
        // Since we are using unsigned shorts, we don't need to worry about <0 (they will wrap to be larger than the map)
        if (x >= mapWidth || y >= mapHeight)
        {
            return;
        }

        auto &node = grid[x + y * mapWidth];

        // Compute the cost of this node
        auto cost = current->cost + (direction % 2 == 1 ? COST_DIAGONAL : COST_STRAIGHT);
        //cost -= std::max((unsigned short)4U, Map::unwalkableProximity(x, y));

        // If the node already has a lower cost, we don't need to consider it
        if (node.cost <= cost) return;

        // Don't allow a diagonal connection from a walkable tile through a blocked tile
        if (direction % 2 == 1 && Map::isWalkable(x, y) &&
            (!allowDiagonalConnectionThrough(x, current->y) || !allowDiagonalConnectionThrough(current->x, y)))
        {
            return;
        }

        // Make the connection if it isn't already done
        node.cost = cost;
        if (node.nextNode != current)
        {
            // Remove the reverse connection if the node was previously connected to another one
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

            // Create the connection
            node.nextNode = current;
            current->prevNodes |= 1U << direction;
        }

        // Queue the node if it is walkable
        if (walkableAndNotMineralLine(x, y))
        {
#if DEBUG_LOG_UPDATES
            Log::Debug() << "Grid-" << goal << ": Queueing " << node;
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
        Log::Debug() << "Grid-" << goal << ": Processing " << *node << "; diag=" << std::get<2>(current);
#endif

        // Boolean controls whether we are considering diagonal edges or straight edges
        if (std::get<2>(current))
        {
            // Diagonals
            visit(node, node->x - 1, node->y + 1, 1);
            visit(node, node->x + 1, node->y + 1, 3);
            visit(node, node->x - 1, node->y - 1, 5);
            visit(node, node->x + 1, node->y - 1, 7);
        }
        else
        {
            // Straight edges
            visit(node, node->x - 1, node->y, 0);
            visit(node, node->x + 1, node->y, 2);
            visit(node, node->x, node->y - 1, 4);
            visit(node, node->x, node->y + 1, 6);
        }
    }

#if OUTPUT_GRID_TIMING
    auto now = std::chrono::high_resolution_clock::now();
    Log::Get() << "Update navigation grid " << goal << ": " << std::chrono::duration_cast<std::chrono::microseconds>(now - start).count() << "us";
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
                Log::Get() << "ERROR: Grid-" << goal << ": Node with no next node " << node;
                return;
            }

            if (node.cost == 0 && node.nextNode)
            {
                Log::Get() << "ERROR: Grid-" << goal << ": Goal node with next node " << node;
                return;
            }

            if (node.nextNode)
            {
                int costDiff = node.cost - node.nextNode->cost;
                if (costDiff <= 0)
                {
                    Log::Get() << "ERROR: Grid-" << goal << ": Non-decreasing cost from " << node << " to " << (*node.nextNode) << ": " << costDiff;
                }
                if (costDiff != COST_STRAIGHT && costDiff != COST_DIAGONAL)
                {
                    Log::Get() << "ERROR: Grid-" << goal << ": Invalid cost from " << node << " to " << (*node.nextNode) << ": " << costDiff;
                    return;
                }
                if (costDiff == COST_DIAGONAL && Map::isWalkable(x, y) &&
                    (!allowDiagonalConnectionThrough(node.nextNode->x, y) || !allowDiagonalConnectionThrough(x, node.nextNode->y)))
                {
                    Log::Get() << "ERROR: Grid-" << goal << ": Diagonal path through blocked tile from " << node << " to " << (*node.nextNode);
                    return;
                }
            }

            auto current = node.nextNode;
            int i = 0;
            while (current != nullptr && i < 5)
            {
                if (current == &node)
                {
                    Log::Get() << "ERROR: Grid-" << goal << ": Loop between " << node << " and " << *current;
                    return;
                }

                if (!Map::isWalkable(current->x, current->y))
                {
                    Log::Get() << "ERROR: Grid-" << goal << ": Path from " << node << " goes through unwalkable tile " << *current;
                    return;
                }

                current = current->nextNode;
                i++;
            }
        }
    }
#endif
}

void NavigationGrid::addBlockingObject(BWAPI::TilePosition tile, BWAPI::TilePosition size)
{
#if DEBUG_LOG_UPDATES
    Log::Debug() << "Grid-" << goal << ": addBlockingObject(" << tile << "," << size << ")";
#endif

    for (int x = tile.x; x < tile.x + size.x; x++)
    {
        for (int y = tile.y; y < tile.y + size.y; y++)
        {
            pendingBlockingTiles.insert(BWAPI::TilePosition(x, y));
        }
    }

    // Add tiles that have diagonal connections through a corner of the object
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
            Log::Debug() << "Grid-" << goal << ": Added corner tile " << BWAPI::TilePosition(toX, toY) << " for direction " << direction;
#endif

            pendingBlockingTiles.insert(BWAPI::TilePosition(toX, toY));
        }
    };
    addCornerTile(tile.x, tile.y - 1, 1); // up-right, top-left corner
    addCornerTile(tile.x + size.x, tile.y + size.y - 1, 1); // up-right, bottom-right corner
    addCornerTile(tile.x + size.x - 1, tile.y - 1, 3); // up-left, top-right corner
    addCornerTile(tile.x - 1, tile.y + size.y - 1, 3); // up-left, bottom-left corner
    addCornerTile(tile.x + size.x, tile.y, 5); // down-right, top-right corner
    addCornerTile(tile.x, tile.y + size.y, 5); // down-right, bottom-left corner
    addCornerTile(tile.x - 1, tile.y, 7); // down-left, top-left corner
    addCornerTile(tile.x + size.x - 1, tile.y + size.y, 7); // down-left, bottom-right corner
}

void NavigationGrid::addBlockingTiles(const std::set<BWAPI::TilePosition> &tiles)
{
    pendingBlockingTiles.insert(tiles.begin(), tiles.end());
}

void NavigationGrid::removeBlockingObject(BWAPI::TilePosition tile, BWAPI::TilePosition size)
{
#if DEBUG_LOG_UPDATES
    Log::Debug() << "Grid-" << goal << ": removeBlockingObject(" << tile << "," << size << ")";
#endif

    std::set<BWAPI::TilePosition> tiles;
    for (int x = tile.x; x < tile.x + size.x; x++)
    {
        for (int y = tile.y; y < tile.y + size.y; y++)
        {
            tiles.insert(BWAPI::TilePosition(x, y));
        }
    }

    removeBlockingTiles(tiles);
}

void NavigationGrid::removeBlockingTiles(const std::set<BWAPI::TilePosition> &tiles)
{
    // Reset the cost of all nodes corresponding to the blocking tiles being removed
    // While doing this, gather tiles that potentially border these blocking tiles
    // Finally add the valid border tiles to the update queue

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
    for (const auto &tile : tiles)
    {
        visit(tile.x, tile.y);
    }

    for (auto node : bordering)
    {
        if (node->nextNode)
        {
#if DEBUG_LOG_UPDATES
            Log::Debug() << "Grid-" << goal << ": removeBlocking, enqueued " << node->tile;
#endif

            nodeQueue.emplace(node->cost + COST_STRAIGHT, node, false);
            nodeQueue.emplace(node->cost + COST_DIAGONAL, node, true);
        }
    }
}

void NavigationGrid::updateBlockingTiles()
{
    if (pendingBlockingTiles.empty()) return;

#if DEBUG_LOG_UPDATES
    Log::Debug() << "Grid-" << goal << ": Updating blocking tiles";
#endif

    // First, invalidate every path that goes through the tiles
    // Then push all still-valid tiles bordering an invalidated tile to the update queue
    // When the grid is updated, all of the invalidated tiles will receive a new path from these bordering tiles

    std::queue<GridNode *> queue;
    for (const auto &tile : pendingBlockingTiles)
    {
        auto &node = grid[tile.x + tile.y * mapWidth];
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

    auto visit = [&](NavigationGrid::GridNode *current, unsigned short x, unsigned short y)
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

    // Push all valid bordering tiles to the update queue
    for (auto &borderingNode : bordering)
    {
        if (borderingNode->nextNode)
        {
#if DEBUG_LOG_UPDATES
            Log::Debug() << "Grid-" << goal << ": updateBlockingTiles, enqueued " << borderingNode->tile;
#endif

            nodeQueue.emplace(borderingNode->cost + COST_STRAIGHT, borderingNode, false);
            nodeQueue.emplace(borderingNode->cost + COST_DIAGONAL, borderingNode, true);
        }
    }

    pendingBlockingTiles.clear();
}

void NavigationGrid::dumpHeatmap()
{
    // Dump to CherryVis
    std::vector<long> costs(mapWidth * mapHeight);
    for (int y = 0; y < mapHeight; y++)
    {
        for (int x = 0; x < mapWidth; x++)
        {
            auto cost = grid[x + y * mapWidth].cost;
            costs[x + y * mapWidth] = cost == USHRT_MAX ? 0 : cost;
        }
    }

    CherryVis::addHeatmap((std::ostringstream() << "Navigation@" << goal).str(), costs, mapWidth, mapHeight);
}
