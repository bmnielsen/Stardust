#include "Block.h"

class Block17x6 : public BuildBlock
{
public:
    Block17x6(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 17; }

    [[nodiscard]] int height() const override { return 6; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tilePos, tileAvailability))
        {
            if (cornerTL.x + 17 < BWAPI::Broodwar->mapWidth())
            {
                return std::make_shared<Block17x6>(tilePos, tilePos + BWAPI::TilePosition(8, 2));
            }
            else
            {
                return std::make_shared<Block17x6>(tilePos, tilePos + BWAPI::TilePosition(7, 2));
            }
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        if (cornerTL.x + 17 < BWAPI::Broodwar->mapWidth())
        {
            small.emplace_back(cornerTL + BWAPI::TilePosition(8, 2));
            small.emplace_back(cornerTL + BWAPI::TilePosition(8, 0));
            small.emplace_back(cornerTL + BWAPI::TilePosition(8, 4));
            large.emplace_back(cornerTL);
            large.emplace_back(cornerTL + BWAPI::TilePosition(0, 3));
            large.emplace_back(cornerTL + BWAPI::TilePosition(4, 0));
            large.emplace_back(cornerTL + BWAPI::TilePosition(4, 3));
            large.emplace_back(cornerTL + BWAPI::TilePosition(10, 0));
            large.emplace_back(cornerTL + BWAPI::TilePosition(10, 3));
            medium.emplace_back(cornerTL + BWAPI::TilePosition(14, 0));
            medium.emplace_back(cornerTL + BWAPI::TilePosition(14, 2));
            medium.emplace_back(cornerTL + BWAPI::TilePosition(14, 4));
            medium.emplace_back(cornerTL + BWAPI::TilePosition(1, 1), true);
            medium.emplace_back(cornerTL + BWAPI::TilePosition(1, 3), true);
            medium.emplace_back(cornerTL + BWAPI::TilePosition(5, 1), true);
            medium.emplace_back(cornerTL + BWAPI::TilePosition(5, 3), true);
            medium.emplace_back(cornerTL + BWAPI::TilePosition(10, 1), true);
            medium.emplace_back(cornerTL + BWAPI::TilePosition(10, 3), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(0, 1), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(2, 1), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(0, 3), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(2, 3), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(4, 1), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(6, 1), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(4, 3), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(6, 3), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(10, 1), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(12, 1), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(10, 3), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(12, 3), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(14, 0), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(14, 2), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(14, 4), true);
        }
        else
        {
            small.emplace_back(cornerTL + BWAPI::TilePosition(7, 2));
            small.emplace_back(cornerTL + BWAPI::TilePosition(7, 0));
            small.emplace_back(cornerTL + BWAPI::TilePosition(7, 4));
            medium.emplace_back(cornerTL);
            medium.emplace_back(cornerTL + BWAPI::TilePosition(0, 2));
            medium.emplace_back(cornerTL + BWAPI::TilePosition(0, 4));
            large.emplace_back(cornerTL + BWAPI::TilePosition(3, 0));
            large.emplace_back(cornerTL + BWAPI::TilePosition(3, 3));
            large.emplace_back(cornerTL + BWAPI::TilePosition(9, 0));
            large.emplace_back(cornerTL + BWAPI::TilePosition(9, 3));
            large.emplace_back(cornerTL + BWAPI::TilePosition(13, 0));
            large.emplace_back(cornerTL + BWAPI::TilePosition(13, 3));
            medium.emplace_back(cornerTL + BWAPI::TilePosition(4, 1), true);
            medium.emplace_back(cornerTL + BWAPI::TilePosition(4, 3), true);
            medium.emplace_back(cornerTL + BWAPI::TilePosition(9, 1), true);
            medium.emplace_back(cornerTL + BWAPI::TilePosition(9, 3), true);
            medium.emplace_back(cornerTL + BWAPI::TilePosition(13, 1), true);
            medium.emplace_back(cornerTL + BWAPI::TilePosition(13, 3), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(1, 0), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(1, 2), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(1, 4), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(3, 1), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(5, 1), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(3, 3), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(5, 3), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(9, 1), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(11, 1), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(9, 3), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(11, 3), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(13, 1), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(15, 1), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(13, 3), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(15, 3), true);
        }
    }
};