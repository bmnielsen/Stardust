#include "NavigationGrid.h"

#include <queue>
#include "Map.h"

#if INSTRUMENTATION_ENABLED
#define NAVIGATION_HEATMAP_ENABLED true
#define OUTPUT_GRID_TIMING false
#endif

namespace
{
    bool walkableAndNotMineralLine(int x, int y)
    {
        return Map::isWalkable(x, y) && !Map::isInOwnMineralLine(x, y);
    }

    bool allowDiagonalConnectionThrough(int x, int y)
    {
        return walkableAndNotMineralLine(x, y) || Map::mapSpecificOverride()->allowDiagonalPathingThrough(x, y);
    }
}

NavigationGrid::NavigationGrid(BWAPI::TilePosition goal, BWAPI::TilePosition goalSize) : goal(goal)
{
    // Create each grid cell with its x,y coordinates
    grid.reserve(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
    for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
    {
        for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
        {
            grid.emplace_back(x, y);
        }
    }

    auto pushInitialTile = [&](BWAPI::TilePosition tile)
    {
        if (!tile.isValid()) return;

        auto &node = (*this)[tile];
        node.cost = 0;
        nodeQueue.push(std::make_tuple(10, &node, false));
        nodeQueue.push(std::make_tuple(14, &node, true));
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
    return grid[(pos.x >> 5U) + (pos.y >> 5U) * BWAPI::Broodwar->mapWidth()];
}

const NavigationGrid::GridNode &NavigationGrid::operator[](BWAPI::Position pos) const
{
    return grid[(pos.x >> 5U) + (pos.y >> 5U) * BWAPI::Broodwar->mapWidth()];
}

NavigationGrid::GridNode &NavigationGrid::operator[](BWAPI::WalkPosition pos)
{
    return grid[(pos.x >> 2U) + (pos.y >> 2U) * BWAPI::Broodwar->mapWidth()];
}

const NavigationGrid::GridNode &NavigationGrid::operator[](BWAPI::WalkPosition pos) const
{
    return grid[(pos.x >> 2U) + (pos.y >> 2U) * BWAPI::Broodwar->mapWidth()];
}

NavigationGrid::GridNode &NavigationGrid::operator[](BWAPI::TilePosition pos)
{
    return grid[pos.x + pos.y * BWAPI::Broodwar->mapWidth()];
}

const NavigationGrid::GridNode &NavigationGrid::operator[](BWAPI::TilePosition pos) const
{
    return grid[pos.x + pos.y * BWAPI::Broodwar->mapWidth()];
}

void NavigationGrid::update()
{
    if (nodeQueue.empty()) return;

#if OUTPUT_GRID_TIMING
    auto start = std::chrono::high_resolution_clock::now();
#endif

    auto visit = [&](NavigationGrid::GridNode *current, unsigned short x, unsigned short y, unsigned char direction)
    {
        // Check validity of the tile
        // Since we are using unsigned shorts, we don't need to worry about <0 (they will wrap to be larger than the map)
        if (x >= BWAPI::Broodwar->mapWidth() || y >= BWAPI::Broodwar->mapHeight())
        {
            return;
        }

        auto &node = grid[x + y * BWAPI::Broodwar->mapWidth()];

        // Compute the cost of this node
        auto cost = current->cost + (direction % 2 == 1 ? 14 : 10);
        cost += 3 - Map::unwalkableProximity(x, y);

        // If the node already has a lower cost, we don't need to consider it
        if (node.cost <= cost) return;

        // Don't allow diagonal connections through blocked tiles
        if (direction % 2 == 1 && (!allowDiagonalConnectionThrough(x, current->y) || !allowDiagonalConnectionThrough(current->x, y))) return;

        // If the target node is already connected, remove the reverse connection
        if (node.nextNode)
        {
            if (node.nextNode->x < node.x)
            {
                if (node.nextNode->y < node.y)
                {
                    node.nextNode->prevNodes &= ~(1U << 5U);
                }
                else if (node.nextNode->y > node.y)
                {
                    node.nextNode->prevNodes &= ~(1U << 1U);
                }
                else
                {
                    node.nextNode->prevNodes &= ~(1U << 0U);
                }
            }
            else if (node.nextNode->x > node.x)
            {
                if (node.nextNode->y < node.y)
                {
                    node.nextNode->prevNodes &= ~(1U << 7U);
                }
                else if (node.nextNode->y > node.y)
                {
                    node.nextNode->prevNodes &= ~(1U << 3U);
                }
                else
                {
                    node.nextNode->prevNodes &= ~(1U << 2U);
                }
            }
            else
            {
                if (node.nextNode->y < node.y)
                {
                    node.nextNode->prevNodes &= ~(1U << 4U);
                }
                else
                {
                    node.nextNode->prevNodes &= ~(1U << 6U);
                }
            }
        }

        // Create the connection
        node.cost = cost;
        node.nextNode = current;
        current->prevNodes |= 1U << direction;

        // Queue the node if it is walkable
        if (walkableAndNotMineralLine(x, y))
        {
            nodeQueue.push(std::make_tuple(node.cost + 10, &node, false));
            nodeQueue.push(std::make_tuple(node.cost + 14, &node, true));
        }
    };

    while (!nodeQueue.empty())
    {
        std::tuple<unsigned short, GridNode *, bool> current = nodeQueue.top();
        nodeQueue.pop();

        auto node = std::get<1>(current);

        if (!walkableAndNotMineralLine(node->x, node->y) || node->cost == USHRT_MAX) continue;

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
}

void NavigationGrid::addBlockingObject(BWAPI::TilePosition tile, BWAPI::TilePosition size)
{
    std::set<BWAPI::TilePosition> tiles;
    for (int x = tile.x; x < tile.x + size.x; x++)
    {
        for (int y = tile.y; y < tile.y + size.y; y++)
        {
            tiles.insert(BWAPI::TilePosition(x, y));
        }
    }

    // Add tiles that might have diagonal connections through the corner of the building
    auto addCornerTile = [&](int offsetX, int offsetY)
    {
        auto here = tile + BWAPI::TilePosition(offsetX, offsetY);
        if (here.isValid()) tiles.insert(here);
    };
    addCornerTile(0, -1);
    addCornerTile(size.x - 1, -1);
    addCornerTile(size.x, 0);
    addCornerTile(size.x, size.y - 1);
    addCornerTile(size.x - 1, size.y);
    addCornerTile(0, size.y);
    addCornerTile(-1, size.y - 1);
    addCornerTile(-1, 0);

    addBlockingTiles(tiles);
}

void NavigationGrid::addBlockingTiles(const std::set<BWAPI::TilePosition> &tiles)
{
    // Step one: add all blocking tiles to a queue and collect all of their neighbours
    // These will act as potential origins for recomputed paths
    std::set<GridNode *> origins;
    auto addOrigin = [&](unsigned short x, unsigned short y)
    {
        if (x >= BWAPI::Broodwar->mapWidth() || y >= BWAPI::Broodwar->mapHeight())
        {
            return;
        }

        origins.insert(&grid[x + y * BWAPI::Broodwar->mapWidth()]);
    };
    std::queue<GridNode *> queue;
    for (const auto &tile : tiles)
    {
        auto &node = grid[tile.x + tile.y * BWAPI::Broodwar->mapWidth()];
        queue.push(&node);

        addOrigin(node.x + 1, node.y);
        addOrigin(node.x - 1, node.y);
        addOrigin(node.x, node.y + 1);
        addOrigin(node.x, node.y - 1);
        addOrigin(node.x - 1, node.y - 1);
        addOrigin(node.x + 1, node.y - 1);
        addOrigin(node.x - 1, node.y + 1);
        addOrigin(node.x + 1, node.y + 1);
    }

    // Step two: invalidate every path that goes through the blocking tiles
    auto visit = [&](NavigationGrid::GridNode *current, unsigned short x, unsigned short y)
    {
        auto &node = grid[x + y * BWAPI::Broodwar->mapWidth()];

        node.cost = USHRT_MAX;
        node.nextNode = nullptr;
        if (node.prevNodes) queue.push(&node);
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

    // Step three: push all valid original next nodes to the update queue for use in assigning new paths
    for (auto &origin : origins)
    {
        if (origin->nextNode)
        {
            nodeQueue.push(std::make_tuple(origin->cost + 10, origin, false));
            nodeQueue.push(std::make_tuple(origin->cost + 14, origin, true));
        }
    }
}

void NavigationGrid::removeBlockingObject(BWAPI::TilePosition tile, BWAPI::TilePosition size)
{
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

    std::vector<GridNode *> bordering;
    auto addBordering = [&](unsigned short x, unsigned short y)
    {
        if (x >= BWAPI::Broodwar->mapWidth() || y >= BWAPI::Broodwar->mapHeight())
        {
            return;
        }

        bordering.push_back(&grid[x + y * BWAPI::Broodwar->mapWidth()]);
    };

    auto visit = [&](unsigned short x, unsigned short y)
    {
        auto &node = grid[x + y * BWAPI::Broodwar->mapWidth()];

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
            nodeQueue.push(std::make_tuple(node->cost + 10, node, false));
            nodeQueue.push(std::make_tuple(node->cost + 14, node, true));
        }
    }
}

void NavigationGrid::dumpHeatmap()
{
#if CHERRYVIS_ENABLED
    // Dump to CherryVis
    std::vector<long> costs(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
    for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
    {
        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
        {
            auto cost = grid[x + y * BWAPI::Broodwar->mapWidth()].cost;
            costs[x + y * BWAPI::Broodwar->mapWidth()] = cost == USHRT_MAX ? 0 : cost;
        }
    }

    CherryVis::addHeatmap((std::ostringstream() << "Navigation@" << goal).str(), costs, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
#endif
}
