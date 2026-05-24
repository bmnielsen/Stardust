#pragma once

#include "Resource.h"

#define PATCH_OCCUPIED_HORIZON 50

namespace MiningOptimization
{
    class PatchOccupiedForecast
    {
    public:
        PatchOccupiedForecast(Resource patch);

    private:
        Resource patch;
        // std::array<double, PATCH_OCCUPIED_HORIZON> forecast = {0.0};
    };
}
