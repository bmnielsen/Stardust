#include "NavigationGrid.h"

#include <queue>
#include "Map.h"

#define OUTPUT_GRID_TIMING true

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

        // If the node already has a lower cost, we don't need to consider it
        auto cost = current->cost + (direction % 2 == 1 ? 14 : 10);
        if (node.cost <= cost) return;

        // Don't allow diagonal connections through blocked tiles
        if (direction % 2 == 1 && !Map::isWalkable(x, current->y) && !Map::isWalkable(current->x, y)) return;

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
        if (Map::isWalkable(x, y))
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

        if (!Map::isWalkable(node->x, node->y) || node->cost == USHRT_MAX) continue;

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

    dumpHeatmap();
}

void NavigationGrid::addBlockingObject(BWAPI::TilePosition tile, BWAPI::TilePosition size)
{
    // Invalidate every path that goes through the object
    std::queue<GridNode *> queue;
    for (int x = tile.x; x < tile.x + size.x; x++)
    {
        for (int y = tile.y; y < tile.y + size.y; y++)
        {
            auto &node = grid[x + y * BWAPI::Broodwar->mapWidth()];
            queue.push(&node);
        }
    }

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

    // Push all valid nodes around the building to the update queue
    auto pushInitialTile = [&](unsigned short x, unsigned short y)
    {
        if (x >= BWAPI::Broodwar->mapWidth() || y >= BWAPI::Broodwar->mapHeight())
        {
            return;
        }

        auto &node = grid[x + y * BWAPI::Broodwar->mapWidth()];
        if (node.nextNode)
        {
            nodeQueue.push(std::make_tuple(node.cost + 10, &node, false));
            nodeQueue.push(std::make_tuple(node.cost + 14, &node, true));
        }
    };

    for (int x = tile.x - 1; x <= tile.x + size.x; x++)
    {
        pushInitialTile(x, tile.y - 1);
        pushInitialTile(x, tile.y + size.y);
    }
    for (int y = tile.y; y < tile.y + size.y; y++)
    {
        pushInitialTile(tile.x - 1, y);
        pushInitialTile(tile.x + size.x, y);
    }
}

void NavigationGrid::removeBlockingObject(BWAPI::TilePosition tile, BWAPI::TilePosition size)
{
    // Reset the cost of the nodes inside the building
    for (int x = tile.x; x < tile.x + size.x; x++)
    {
        for (int y = tile.y; y < tile.y + size.y; y++)
        {
            auto &node = grid[x + y * BWAPI::Broodwar->mapWidth()];
            node.cost = USHRT_MAX;
        }
    }

    // Push all valid nodes around the building to the update queue
    auto pushInitialTile = [&](unsigned short x, unsigned short y)
    {
        if (x >= BWAPI::Broodwar->mapWidth() || y >= BWAPI::Broodwar->mapHeight())
        {
            return;
        }

        auto &node = grid[x + y * BWAPI::Broodwar->mapWidth()];
        if (node.nextNode)
        {
            nodeQueue.push(std::make_tuple(node.cost + 10, &node, false));
            nodeQueue.push(std::make_tuple(node.cost + 14, &node, true));
        }
    };

    for (int x = tile.x - 1; x <= tile.x + size.x; x++)
    {
        pushInitialTile(x, tile.y - 1);
        pushInitialTile(x, tile.y + size.y);
    }
    for (int y = tile.y; y < tile.y + size.y; y++)
    {
        pushInitialTile(tile.x - 1, y);
        pushInitialTile(tile.x + size.x, y);
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