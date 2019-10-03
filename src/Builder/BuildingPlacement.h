#pragma once

#include "Common.h"

namespace BuildingPlacement
{
    struct BuildLocation;
    struct BuildLocationCmp { bool operator()(const BuildLocation & a, const BuildLocation & b) const; };

    typedef std::set<BuildLocation, BuildLocationCmp> BuildLocationSet;

    // Small struct to hold data about a build location
    struct BuildLocation
    {
        BWAPI::TilePosition tile;                  // The position
        int                 builderFrames;         // Approximately how many frames the builder will take to get to this location
        int                 framesUntilPowered;    // Approximately how many frames will elapse before this position is powered
        BuildLocationSet    powersMedium;          // For a pylon, what medium build locations would be powered by it
        BuildLocationSet    powersLarge;           // For a pylon, what large build locations would be powered by it

        BuildLocation(BWAPI::TilePosition tile, int builderFrames, int framesUntilPowered)
            : tile(tile)
            , builderFrames(builderFrames)
            , framesUntilPowered(framesUntilPowered)
        {}
    };

    enum class Neighbourhood { MainBase }; // TODO: Add proxy, etc.

    void initialize();

    void onUnitDestroy(BWAPI::Unit unit);
    void onUnitMorph(BWAPI::Unit unit);
    void onUnitCreate(BWAPI::Unit unit);
    void onUnitDiscover(BWAPI::Unit unit);

    void update();

    std::map<Neighbourhood, std::map<int, BuildLocationSet>> & getBuildLocations();

    BuildLocationSet & availableGeysers();

    int builderFrames(Neighbourhood location, BWAPI::TilePosition tile, BWAPI::UnitType type);
}