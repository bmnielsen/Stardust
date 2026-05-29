#include "OtherPatchesOccupiedForecast.h"

namespace MiningOptimization
{
    OtherPatchesOccupiedForecast::OtherPatchesOccupiedForecast(const Resource &patch,
                                                               const std::map<Resource, PatchOccupiedForecast> &patchForecasts)
    {
        for (auto &other : patch->resourcesInSwitchPatchRange)
        {
            if (other->destroyed) continue;

            auto it = patchForecasts.find(other);
            if (it == patchForecasts.end())
            {
                Log::Get() << "WARNING: Patch in switch patch range not in forecast list: " << *other;
                continue;
            }

            if (it->second.startFrame != currentFrame)
            {
                Log::Get() << "ERROR: OtherPatchesOccupiedForecast can only be used with forecasts made from the current frame";
                continue;
            }

            if (it->second.fullySaturated) continue;

            std::transform(forecast.begin(),
                           forecast.end(),
                           it->second.forecast.begin(),
                           forecast.begin(),
                           std::multiplies<>{});
        }
    }
}
