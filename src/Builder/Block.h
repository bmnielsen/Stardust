#pragma once

#include "Common.h"

// Stores information about a block of build locations.
class Block
{
public:
    struct Location
    {
        BWAPI::TilePosition tile;
        bool converted; // Whether this position is converted from a more-efficient position
        bool hasExit; // Whether this position has an exit (only applicable to medium locations)

        explicit Location(BWAPI::TilePosition tile, bool converted = false, bool hasExit = true)
                : tile(tile), converted(converted), hasExit(hasExit) {}
    };

    BWAPI::TilePosition topLeft;

    BWAPI::TilePosition powerPylon;

    std::vector<Location> small;
    std::vector<Location> medium;
    std::vector<Location> large;

    std::vector<BWAPI::TilePosition> cannons; // Only applicable to start blocks

    Block(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : topLeft(topLeft), powerPylon(powerPylon) {}

    virtual ~Block() = default;

    [[nodiscard]] virtual int width() const = 0;

    [[nodiscard]] virtual int height() const = 0;

    [[nodiscard]] BWAPI::Position center() const;

    [[nodiscard]] virtual bool allowTopEdge() const { return true; }

    [[nodiscard]] virtual bool allowLeftEdge() const { return true; }

    [[nodiscard]] virtual bool allowRightEdge() const { return true; }

    [[nodiscard]] virtual bool allowCorner() const { return true; }

    bool tilesReserved(BWAPI::TilePosition tile, BWAPI::TilePosition size, bool permanent = false);

    virtual bool tilesUsed(BWAPI::TilePosition tile, BWAPI::TilePosition size);

    bool tilesFreed(BWAPI::TilePosition tile, BWAPI::TilePosition size);

    virtual std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) = 0;

protected:
    bool place(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability);

    bool placeStartBlock(std::vector<BWAPI::TilePosition> &usedTiles,
                         std::vector<BWAPI::TilePosition> &borderTiles,
                         std::vector<unsigned int> &tileAvailability);

    virtual void placeLocations() = 0;

    void removeUsed();

private:
    std::vector<std::pair<BWAPI::TilePosition, BWAPI::TilePosition>> permanentTileReservations;
};
