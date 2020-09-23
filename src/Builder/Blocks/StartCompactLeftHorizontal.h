#include "Block.h"

class StartCompactLeftHorizontal : public Block
{
public:
    explicit StartCompactLeftHorizontal(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 8; }

    [[nodiscard]] int height() const override { return 5; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        // For start blocks the provided tile is the nexus
        auto blockTile = tile + BWAPI::TilePosition(-8, -2);

        // Generate block tiles
        std::vector<BWAPI::TilePosition> usedTiles;
        std::vector<BWAPI::TilePosition> borderTiles;
        for (int tileX = blockTile.x - 1; tileX <= blockTile.x + 8; tileX++)
        {
            for (int tileY = blockTile.y - 1; tileY <= blockTile.y + 5; tileY++)
            {
                if (tileX < 0 || tileY < 0 || tileX >= BWAPI::Broodwar->mapWidth() || tileY >= BWAPI::Broodwar->mapHeight()) return nullptr;

                if (tileX == blockTile.x - 1 || tileY == blockTile.y - 1 || tileX == blockTile.x + 8 || tileY == blockTile.y + 5)
                {
                    borderTiles.emplace_back(BWAPI::TilePosition(tileX, tileY));
                }
                else
                {
                    usedTiles.emplace_back(BWAPI::TilePosition(tileX, tileY));
                }
            }
        }

        if (placeStartBlock(usedTiles, borderTiles, tileAvailability))
        {
            return std::make_shared<StartCompactLeftHorizontal>(blockTile, blockTile + BWAPI::TilePosition(6, 3));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft + BWAPI::TilePosition(6, 3));
        medium.emplace_back(topLeft + BWAPI::TilePosition(0, 3));
        medium.emplace_back(topLeft + BWAPI::TilePosition(3, 3));
        large.emplace_back(topLeft);
        large.emplace_back(topLeft + BWAPI::TilePosition(4, 0));

        cannons.emplace_back(topLeft + BWAPI::TilePosition(12, 1));
        cannons.emplace_back(topLeft + BWAPI::TilePosition(12, 5));
        cannons.emplace_back(topLeft + BWAPI::TilePosition(8, 5));
        cannons.emplace_back(topLeft + BWAPI::TilePosition(8, 0));
    }
};