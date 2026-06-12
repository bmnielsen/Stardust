#pragma once

#include "MyWorker.h"
#include "Resource.h"

#define PATCH_OCCUPIED_HORIZON 50

namespace MiningOptimization
{
    class PatchOccupiedForecast
    {
    public:
        friend class OtherPatchesOccupiedForecast;

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

        MyWorker miningWorker;
        MyWorker nextMiningWorker;

        std::optional<int> miningWorkerLatestEndFrame;

        [[nodiscard]] double atFrame(int frame) const
        {
            int frameIdx = frame - startFrame - 1;
            if (frameIdx < 0 || frameIdx >= GATHER_FORECAST_FRAMES) return 0.0;
            return fullySaturated ? 1.0 : forecast[frameIdx];
        }

        // Update the forecast for takeover at the given frame
        void useTakeoverFrame(int frame);

        // Update the forecast for patch locking at the given frame
        void usePatchLockFrame(int frame);

    private:
        Resource patch;
        int startFrame;

        // If true, the forecast is for the patch to be mined through the entire forecast horizon
        bool fullySaturated = false;

        std::array<double, PATCH_OCCUPIED_HORIZON> forecast = {0.0};
    };
}
