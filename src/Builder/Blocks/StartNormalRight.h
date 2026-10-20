#include "Block.h"

class StartNormalRight : public BuildBlock
{
public:
    explicit StartNormalRight(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 8; }

    [[nodiscard]] int height() const override { return 7; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        /* For start blocks the provided tile is the nexus */
        auto blkTile = tilePos + BWAPI::TilePosition(4, -2);

        /* Generate block tiles */
        std::vector<BWAPI::TilePosition> usedTiles;
        std::vector<BWAPI::TilePosition> borderTiles;
        for (int tileX = blkTile.x - 1; tileX <= blkTile.x + 8; tileX++)
        {
            for (int tileY = blkTile.y - 1; tileY <= blkTile.y + 7; tileY++)
            {
                if (tileX < 0 || tileY < 0 || tileX >= BWAPI::Broodwar->mapWidth() || tileY >= BWAPI::Broodwar->mapHeight()) return nullptr;

                if (tileX == blkTile.x - 1 || tileY == blkTile.y - 1 || tileX == blkTile.x + 8 || tileY == blkTile.y + 7)
                {
                    borderTiles.emplace_back(tileX, tileY);
                }
                else
                {
                    usedTiles.emplace_back(tileX, tileY);
                }
            }
        }

        if (placeStartBlock(tilePos, usedTiles, borderTiles, tileAvailability))
        {
            return std::make_shared<StartNormalRight>(blkTile, blkTile + BWAPI::TilePosition(0, 2));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(cornerTL + BWAPI::TilePosition(0, 2));
        small.emplace_back(cornerTL);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(2, 2), false, false);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(5, 2));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(2, 0));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(5, 0));
        large.emplace_back(cornerTL + BWAPI::TilePosition(0, 4));
        large.emplace_back(cornerTL + BWAPI::TilePosition(4, 4));

        cannons.emplace_back(cornerTL + BWAPI::TilePosition(-6, 5));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(-6, 0));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(-2, 5));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(-2, 0));
    }
};