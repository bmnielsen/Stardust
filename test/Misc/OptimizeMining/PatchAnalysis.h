#pragma once

#include "StartingPositionAnalysis.h"

#include <set>

class PatchAnalysis
{
public:
    PatchAnalysis(BWAPI::TilePosition patchTile);

    BWAPI::TilePosition depotTile;
    BWAPI::TilePosition patchTile;
    std::vector<BWAPI::Position> probeStartingPositions;
    std::map<BWAPI::Position, StartingPositionAnalysis> startingPositionAnalysis;

    // States while running the analysis
    int state;
    int lastStateChange;
    BWAPI::Unit worker;
    bool complete;

    void onFrame(int batch, const std::string &dataBasePath);

    static void clearOutputFiles(const std::string &dataBasePath, const std::string &mapHash);

    static std::set<BWAPI::TilePosition> getAnalyzedPatches(const std::string &dataBasePath, const std::string &mapHash);

    void dumpResults(int batch, const std::string &dataBasePath);

    BWAPI::Position depotCenter() const
    {
        return BWAPI::Position(depotTile) + BWAPI::Position(64, 48);
    }
};
