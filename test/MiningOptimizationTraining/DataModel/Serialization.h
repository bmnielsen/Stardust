#pragma once

#include "MapData.h"

namespace MiningOptimizationTraining::Serialization
{
    // Allows setting the game parameters if using the data outside a game (like when doing post-processing on the data files)
    void setGameParameters(const std::string &mapHash);

    void readMapData(MapData &data);
    void writeMapData(MapData &data);

    void readMapData(InitialWorkerMapData &data);
    void writeMapData(InitialWorkerMapData &data);
}
