#pragma once

#include "PatchOccupiedForecast.h"

namespace MiningOptimization
{
    class OtherPatchesOccupiedForecast
    {
    public:
        OtherPatchesOccupiedForecast(const Resource &patch, const std::map<Resource, PatchOccupiedForecast> &patchForecasts);

        friend std::ostream &operator<<(std::ostream &out, const OtherPatchesOccupiedForecast &f)
        {
            std::ostringstream os;
            os << std::fixed << std::setprecision(2);
            std::string sep;
            for (int i = 0; i < std::min(10, PATCH_OCCUPIED_HORIZON); i++)
            {
                os << sep << f.forecast[i];
                sep = ", ";
            }
            out << os.str();
            return out;
        }

        [[nodiscard]] double atFrame(int frame) const
        {
            // The probabilities computed in the constructor are the probabilities at the end of each frame.
            // However, if we want to be sure all other patches are mined when a worker's orders are processed, we need to consider the fact
            // that other workers' orders are likely to have been processed before it.
            // The simplest way to fix this is to include the probability of the other patches also being mined on the previous frame. This takes
            // into account any patches that are forecasted to start being mined on the frame when our worker's orders are processed.
            // A more accurate solution to this would actually consider the order process index, but this would require tracking a lot of
            // additional data and would likely make very little difference in practice.
            // The first frame in the forecast can not be multiplied by the previous, so we leave it as-is, but as a frame that early would never
            // be usable for planning (because of latency), this shouldn't be an issue.

            int frameIdx = frame - currentFrame - 1;
            if (frameIdx < 0 || frameIdx >= GATHER_FORECAST_FRAMES) return 0.0;
            if (frameIdx == 0) return forecast[frameIdx];
            return forecast[frameIdx] * forecast[frameIdx - 1];
        }

    private:
        std::array<double, PATCH_OCCUPIED_HORIZON> forecast;
    };
}
