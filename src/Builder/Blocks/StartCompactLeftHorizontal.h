#include "Block.h"

class StartCompactLeftHorizontal : public BuildBlock
{
public:
    explicit StartCompactLeftHorizontal(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL,
                                                                                                             powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 8; }

    [[nodiscard]] int height() const override { return 5; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        /* For start blocks the provided tile is the nexus */
        auto blkTile = tilePos + BWAPI::TilePosition(-8, -2);

        /* Generate block tiles */
        std::vector<BWAPI::TilePosition> usedTiles;
        std::vector<BWAPI::TilePosition> borderTiles;
        for (int tileX = blkTile.x - 1; tileX <= blkTile.x + 8; tileX++)
        {
            for (int tileY = blkTile.y - 1; tileY <= blkTile.y + 5; tileY++)
            {
                if (tileX < 0 || tileY < 0 || tileX >= BWAPI::Broodwar->mapWidth() || tileY >= BWAPI::Broodwar->mapHeight()) return nullptr;

                if (tileX == blkTile.x - 1 || tileY == blkTile.y - 1 || tileX == blkTile.x + 8 || tileY == blkTile.y + 5)
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
            return std::make_shared<StartCompactLeftHorizontal>(blkTile, blkTile + BWAPI::TilePosition(6, 3));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(cornerTL + BWAPI::TilePosition(6, 3));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(0, 3));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(3, 3));
        large.emplace_back(cornerTL);
        large.emplace_back(cornerTL + BWAPI::TilePosition(4, 0));

        cannons.emplace_back(cornerTL + BWAPI::TilePosition(12, 1));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(12, 5));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(8, 5));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(8, 0));
    }
};