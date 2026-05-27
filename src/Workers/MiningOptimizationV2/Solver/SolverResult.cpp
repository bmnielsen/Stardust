#include "SolverResult.h"

#include "../DataModel/MapData.h"
#include "LogFormattingUtil.h"

#define EPSILON 0.000001

namespace MiningOptimization
{
    template <typename ObservationType>
    std::optional<std::set<int>> SolverResult<ObservationType>::aggregatedResendFramesIfStable() const
    {
        if (nextBranches.empty()) return resendFramesOnThisBranch;

        auto firstResult = nextBranches.begin()->aggregatedResendFramesIfStable();
        if (!firstResult) return std::nullopt;

        if (nextBranches.size() > 1)
        {
            for (auto nextBranchIt = nextBranches.begin() + 1; nextBranchIt != nextBranches.end(); nextBranchIt++)
            {
                auto nextBranchResult = nextBranchIt->aggregatedResendFramesIfStable();
                if (!nextBranchResult) return std::nullopt;
                if (*nextBranchResult != *firstResult) return std::nullopt;
            }
        }

        firstResult->insert(resendFramesOnThisBranch.begin(), resendFramesOnThisBranch.end());
        return firstResult;
    }

    template <typename ObservationType>
    [[nodiscard]] bool SolverResult<ObservationType>::containsArrivalFrame(int arrivalFrame) const
    {
        for (const auto &[arrivalData, _] : arrivalDataWithProbabilities)
        {
            if (arrivalData.arrivalFrame == arrivalFrame) return true;
        }
        return false;
    }

    template <typename ObservationType>
    [[nodiscard]] int SolverResult<ObservationType>::latestArrivalFrame() const
    {
        int result = -1;
        for (const auto &[arrivalData, _] : arrivalDataWithProbabilities)
        {
            result = std::max(result, arrivalData.arrivalFrame);
        }
        return result;
    }

    template <typename ObservationType>
    std::string SolverResult<ObservationType>::framePredictions() const
    {
        std::ostringstream out;
        out << "Arrival frames: " << LogFormattingUtil::formatProbabilityMap(arrivalDataWithProbabilities);
        out << "\nAction frames: " << LogFormattingUtil::formatProbabilityMap(actionFramesWithProbabilities);
        out << "\nPost-action delays: " << LogFormattingUtil::formatProbabilityMap(delaysWithProbabilities);
        return out.str();
    }

    template <typename ObservationType>
    double SolverResult<ObservationType>::mapAverage(const std::map<int, double> &map)
    {
#if LOGGING_ENABLED
        double sum = probabilitySum(map);
        if (sum < (1.0 - EPSILON) || sum > (1.0 + EPSILON))
        {
            Log::Get() << "ERROR: Probabilities don't sum to 1; actual value is " << sum;
        }
#endif

        double result = 0.0;
        for (const auto &[value, probability] : map)
        {
            result += (double)value * probability;
        }
        return result;
    }

    template <typename ObservationType>
    double SolverResult<ObservationType>::probabilitySum(const std::map<int, double> &map)
    {
        double result = 0.0;
        for (const auto &[_, probability] : map) result += probability;
        return result;
    }

    template struct SolverResult<GatherArrivalData>;
    template struct SolverResult<ReturnArrivalData>;
}
