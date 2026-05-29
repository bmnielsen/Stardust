#pragma once

#include "Resource.h"

#define PATCH_OCCUPIED_HORIZON 50

namespace MiningOptimization
{
    class PatchOccupiedForecast
    {
    public:
        PatchOccupiedForecast(Resource patch);

        friend std::ostream &operator<<(std::ostream &out, const PatchOccupiedForecast &f)
        {
            if (f.fullySaturated)
            {
                out << "1.0...";
                return out;
            }

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

        bool fullySaturated = false;

        std::optional<int> miningWorkerOrderProcessIndex;
        std::optional<int> miningWorkerEarliestEndFrame;
        std::optional<int> miningWorkerLatestEndFrame;

        [[nodiscard]] double atFrame(int frame) const
        {
            int frameIdx = frame - startFrame - 1;
            if (frameIdx < 0 || frameIdx >= GATHER_FORECAST_FRAMES) return 0.0;
            return fullySaturated ? 1.0 : forecast[frameIdx];
        }

        void useTakeoverFrame(int frame);

    private:
        Resource patch;
        int startFrame;
        std::array<double, PATCH_OCCUPIED_HORIZON> forecast = {0.0};
    };
}
