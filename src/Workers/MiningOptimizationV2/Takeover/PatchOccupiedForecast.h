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
                os << sep << f.start[i];
                if (f.orderProcessIndices.size() == 2) os << "|" << f.middle[i];
                os << "|" << f.end[i];
                sep = ", ";
            }
            out << os.str();
            return out;
        }

        MyWorker miningWorker;
        MyWorker nextMiningWorker;

        std::optional<int> miningWorkerLatestEndFrame;

        // Update the forecast for takeover by the next mining worker at the given frame
        // The given frame should correspond to the frame where the worker transitions to WaitForMinerals
        void useTakeoverFrame(int frame);

        // Update the forecast for patch locking by the next mining worker at the given frame
        // The given frame should correspond to the frame where the worker transitions to WaitForMinerals
        void usePatchLockFrame(int frame);

    private:
        Resource patch;
        int startFrame;

        // If true, the forecast is for the patch to be mined through the entire forecast horizon
        bool fullySaturated = false;

        // The order process indices of the worker(s), used to determine which forecast array is relevant for aggregations
        std::set<int> orderProcessIndices;

        // The forecast before the first worker's orders are processed
        std::array<double, PATCH_OCCUPIED_HORIZON> start = {0.0};

        // The forecast between the two workers' orders being processed
        // Unused if there is only one worker
        std::array<double, PATCH_OCCUPIED_HORIZON> middle = {0.0};

        // The forecast after the second worker's orders are processed
        std::array<double, PATCH_OCCUPIED_HORIZON> end = {0.0};
    };
}
