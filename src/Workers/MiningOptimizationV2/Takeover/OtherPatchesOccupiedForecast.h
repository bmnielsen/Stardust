#pragma once

#include "PatchOccupiedForecast.h"

namespace MiningOptimization
{
    class OtherPatchesOccupiedForecast
    {
    public:
        OtherPatchesOccupiedForecast(const Resource &patch,
                                     const std::map<Resource, PatchOccupiedForecast> &patchForecasts,
                                     const MyWorker &takingOverWorker);

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
            int frameIdx = frame - currentFrame - 1;
            if (frameIdx < 0 || frameIdx >= PATCH_OCCUPIED_HORIZON) return 0.0;
            return forecast[frameIdx];
        }

    private:
        std::array<double, PATCH_OCCUPIED_HORIZON> forecast = {};
    };
}
