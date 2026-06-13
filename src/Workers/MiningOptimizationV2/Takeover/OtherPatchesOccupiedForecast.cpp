#include "OtherPatchesOccupiedForecast.h"

namespace MiningOptimization
{
    OtherPatchesOccupiedForecast::OtherPatchesOccupiedForecast(const Resource &patch,
                                                               const std::map<Resource, PatchOccupiedForecast> &patchForecasts,
                                                               const MyWorker &takingOverWorker)
    {
        std::fill_n(forecast.begin(), PATCH_OCCUPIED_HORIZON, 1.0);

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

            auto apply = [&](const std::array<double, PATCH_OCCUPIED_HORIZON> &patchForecast)
            {
                std::transform(forecast.begin(),
                           forecast.end(),
                           patchForecast.begin(),
                           forecast.begin(),
                           std::multiplies<>{});
            };

            // Handle all of the different potential order process index combinations
            if (it->second.orderProcessIndices.empty())
            {
                // The other patch has no workers, so it will never be occupied and we can short circuit now
                forecast = {};
                return;
            }
            if (takingOverWorker->orderProcessIndex > *it->second.orderProcessIndices.rbegin())
            {
                // Taking over worker is processed first: look at the start
                apply(it->second.start);
            }
            else if (takingOverWorker->orderProcessIndex < *it->second.orderProcessIndices.begin())
            {
                // Taking over worker is processed last: look at the end
                apply(it->second.end);
            }
            else if (takingOverWorker->orderProcessIndex == *it->second.orderProcessIndices.rbegin())
            {
                // Taking over worker is processed first or second
                // Multiply the first and second sets of probabilities to get the combined view
                if (it->second.orderProcessIndices.size() == 1)
                {
                    apply(it->second.start);
                    apply(it->second.end);
                }
                else
                {
                    apply(it->second.start);
                    apply(it->second.middle);
                }
            }
            else if (takingOverWorker->orderProcessIndex == *it->second.orderProcessIndices.begin())
            {
                // Taking over worker is processed second or third
                // Similar to last branch but here we know there must be two indices
                apply(it->second.middle);
                apply(it->second.end);
            }
            else
            {
                // Taking over worker is processed between the others
                apply(it->second.middle);
            }
        }
    }
}
