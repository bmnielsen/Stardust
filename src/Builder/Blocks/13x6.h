#include "Block.h"

class Block13x6 : public Block
{
public:
    Block13x6(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 13; }

    [[nodiscard]] int height() const override { return 6; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tile, tileAvailability))
        {
            if (topLeft.x + 17 < BWAPI::Broodwar->mapWidth())
            {
                return std::make_shared<Block13x6>(tile, tile + BWAPI::TilePosition(8, 2));
            }
            else
            {
                return std::make_shared<Block13x6>(tile, tile + BWAPI::TilePosition(7, 2));
            }
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        if (topLeft.x + 17 < BWAPI::Broodwar->mapWidth())
        {
            small.emplace_back(topLeft + BWAPI::TilePosition(8, 2));
            small.emplace_back(topLeft + BWAPI::TilePosition(8, 0));
            small.emplace_back(topLeft + BWAPI::TilePosition(8, 4));
            large.emplace_back(topLeft);
            large.emplace_back(topLeft + BWAPI::TilePosition(0, 3));
            large.emplace_back(topLeft + BWAPI::TilePosition(4, 0));
            large.emplace_back(topLeft + BWAPI::TilePosition(4, 3));
            medium.emplace_back(topLeft + BWAPI::TilePosition(10, 0));
            medium.emplace_back(topLeft + BWAPI::TilePosition(10, 2));
            medium.emplace_back(topLeft + BWAPI::TilePosition(10, 4));
            medium.emplace_back(topLeft + BWAPI::TilePosition(1, 1), true);
            medium.emplace_back(topLeft + BWAPI::TilePosition(1, 3), true);
            medium.emplace_back(topLeft + BWAPI::TilePosition(5, 1), true);
            medium.emplace_back(topLeft + BWAPI::TilePosition(5, 3), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(0, 1), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(2, 1), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(0, 3), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(2, 3), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(4, 1), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(6, 1), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(4, 3), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(6, 3), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(10, 0), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(10, 2), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(10, 4), true);
        }
        else
        {
            small.emplace_back(topLeft + BWAPI::TilePosition(7, 2));
            small.emplace_back(topLeft + BWAPI::TilePosition(7, 0));
            small.emplace_back(topLeft + BWAPI::TilePosition(7, 4));
            medium.emplace_back(topLeft);
            medium.emplace_back(topLeft + BWAPI::TilePosition(0, 2));
            medium.emplace_back(topLeft + BWAPI::TilePosition(0, 4));
            large.emplace_back(topLeft + BWAPI::TilePosition(3, 0));
            large.emplace_back(topLeft + BWAPI::TilePosition(3, 3));
            large.emplace_back(topLeft + BWAPI::TilePosition(9, 0));
            large.emplace_back(topLeft + BWAPI::TilePosition(9, 3));
            medium.emplace_back(topLeft + BWAPI::TilePosition(4, 1), true);
            medium.emplace_back(topLeft + BWAPI::TilePosition(4, 3), true);
            medium.emplace_back(topLeft + BWAPI::TilePosition(9, 1), true);
            medium.emplace_back(topLeft + BWAPI::TilePosition(9, 3), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(1, 0), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(1, 2), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(1, 4), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(3, 1), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(5, 1), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(3, 3), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(5, 3), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(9, 1), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(11, 1), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(9, 3), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(11, 3), true);
        }
    }
};