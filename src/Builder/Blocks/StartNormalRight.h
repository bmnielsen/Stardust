#include "Block.h"

class StartNormalRight : public Block
{
public:
    explicit StartNormalRight(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 8; }

    [[nodiscard]] int height() const override { return 7; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        // For start blocks the provided tile is the nexus
        auto blockTile = tile + BWAPI::TilePosition(4, -2);

        // Generate block tiles
        std::vector<BWAPI::TilePosition> usedTiles;
        std::vector<BWAPI::TilePosition> borderTiles;
        for (int tileX = blockTile.x - 1; tileX <= blockTile.x + 8; tileX++)
        {
            for (int tileY = blockTile.y - 1; tileY <= blockTile.y + 7; tileY++)
            {
                if (tileX < 0 || tileY < 0 || tileX >= BWAPI::Broodwar->mapWidth() || tileY >= BWAPI::Broodwar->mapHeight()) return nullptr;

                if (tileX == blockTile.x - 1 || tileY == blockTile.y - 1 || tileX == blockTile.x + 8 || tileY == blockTile.y + 7)
                {
                    borderTiles.emplace_back(tileX, tileY);
                }
                else
                {
                    usedTiles.emplace_back(tileX, tileY);
                }
            }
        }

        if (placeStartBlock(usedTiles, borderTiles, tileAvailability))
        {
            return std::make_shared<StartNormalRight>(blockTile, blockTile + BWAPI::TilePosition(0, 2));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 2));
        small.emplace_back(topLeft);
        medium.emplace_back(topLeft + BWAPI::TilePosition(2, 2), false, false);
        medium.emplace_back(topLeft + BWAPI::TilePosition(5, 2));
        medium.emplace_back(topLeft + BWAPI::TilePosition(2, 0));
        medium.emplace_back(topLeft + BWAPI::TilePosition(5, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(0, 4));
        large.emplace_back(topLeft + BWAPI::TilePosition(4, 4));

        cannons.emplace_back(topLeft + BWAPI::TilePosition(-6, 5));
        cannons.emplace_back(topLeft + BWAPI::TilePosition(-6, 0));
        cannons.emplace_back(topLeft + BWAPI::TilePosition(-2, 5));
        cannons.emplace_back(topLeft + BWAPI::TilePosition(-2, 0));
    }
};