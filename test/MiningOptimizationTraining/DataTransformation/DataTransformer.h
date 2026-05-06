#pragma once

#include "MiningOptimizationTraining/DataModel/MapData.h"

namespace MiningOptimizationTraining::DataTransformer
{
    void transform(const MapData &trainingData, const InitialWorkerMapData &initialWorkerTrainingData);
}
