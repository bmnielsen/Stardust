#include "Block.h"

class Block12x8 : public Block
{
public:
    Block12x8(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 12; }

    [[nodiscard]] int height() const override { return 8; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tile, tileAvailability))
        {
            if (topLeft.x + 12 < BWAPI::Broodwar->mapWidth())
            {
                return std::make_shared<Block12x8>(tile, tile + BWAPI::TilePosition(6, 3));
            }
            else {
                return std::make_shared<Block12x8>(tile, tile + BWAPI::TilePosition(4, 3));
            }
        }

        return nullptr;
    }

    bool tilesUsed(BWAPI::TilePosition tile, BWAPI::TilePosition size) override
    {
        if (!Block::tilesUsed(tile, size)) return false;

        // When the first medium location is taken to the left, it opens up the next one
        if (tile == (topLeft + BWAPI::TilePosition(3, 3)) && size.x == 3 && size.y == 2)
        {
            medium.emplace_back(topLeft + BWAPI::TilePosition(0, 3));
            small.emplace_back(topLeft + BWAPI::TilePosition(1, 3), true);
            removeUsed();
            return true;
        }

        // When the first medium location is taken to the right, it opens up the next one
        if (tile == (topLeft + BWAPI::TilePosition(6, 3)) && size.x == 3 && size.y == 2)
        {
            medium.emplace_back(topLeft + BWAPI::TilePosition(9, 3));
            small.emplace_back(topLeft + BWAPI::TilePosition(9, 3), true);
            removeUsed();
            return true;
        }

        // If the first location on the left is instead taken with a converted pylon, it opens up two more pylons
        if (tile == (topLeft + BWAPI::TilePosition(4, 3)) && size.x == 2 && size.y == 2)
        {
            small.emplace_back(topLeft + BWAPI::TilePosition(2, 3));
            small.emplace_back(topLeft + BWAPI::TilePosition(0, 3));
            removeUsed();
            return true;
        }

        // If the first location on the right is instead taken with a converted pylon, it opens up two more pylons
        if (tile == (topLeft + BWAPI::TilePosition(6, 3)) && size.x == 2 && size.y == 2)
        {
            small.emplace_back(topLeft + BWAPI::TilePosition(8, 3));
            small.emplace_back(topLeft + BWAPI::TilePosition(10, 3));
            removeUsed();
            return true;
        }

        return true;
    }

protected:
    void placeLocations() override
    {
        if (topLeft.x + 12 < BWAPI::Broodwar->mapWidth())
        {
            small.emplace_back(topLeft + BWAPI::TilePosition(6, 3));
            small.emplace_back(topLeft + BWAPI::TilePosition(8, 3));
            small.emplace_back(topLeft + BWAPI::TilePosition(10, 3));
            medium.emplace_back(topLeft + BWAPI::TilePosition(3, 3), false, false);
            small.emplace_back(topLeft + BWAPI::TilePosition(3, 3), true);
        }
        else
        {
            small.emplace_back(topLeft + BWAPI::TilePosition(4, 3));
            small.emplace_back(topLeft + BWAPI::TilePosition(2, 3));
            small.emplace_back(topLeft + BWAPI::TilePosition(0, 3));
            medium.emplace_back(topLeft + BWAPI::TilePosition(6, 3), false, false);
            small.emplace_back(topLeft + BWAPI::TilePosition(6, 3), true);
        }

        large.emplace_back(topLeft);
        large.emplace_back(topLeft + BWAPI::TilePosition(0, 5));
        large.emplace_back(topLeft + BWAPI::TilePosition(4, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(4, 5));
        large.emplace_back(topLeft + BWAPI::TilePosition(8, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(8, 5));
        medium.emplace_back(topLeft + BWAPI::TilePosition(1, 1), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(1, 5), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(5, 1), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(5, 5), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(8, 1), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(8, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(4, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(6, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(4, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(6, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(8, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(10, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(8, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(10, 5), true);
    }
};