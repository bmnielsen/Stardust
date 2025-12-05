#include "SolverResult.h"

#include "../DataModel/MapData.h"

#define EPSILON 0.000001

namespace MiningOptimization
{
    template <typename ObservationType>
    std::string SolverResult<ObservationType>::framePredictions() const
    {
        auto handleMap = [](const std::map<int, double> &map)
        {
            // Sort by descending probability
            std::vector<std::pair<int, double>> values(map.begin(), map.end());
            std::sort(values.begin(), values.end(), [&](const std::pair<int, double> &a, const std::pair<int, double> &b) -> bool
            {
                return a.second > b.second;
            });

            // Output the top three
            std::ostringstream out;
            out << std::fixed << std::setprecision(2);
            std::string sep;
            for (auto it = values.begin(); it != values.end() && std::distance(values.begin(), it) < 3; it++)
            {
                out << sep << it->first << " (" << (it->second * 100.0) << "%)";
                sep = ", ";
            }
            return out.str();
        };

        std::ostringstream out;
        out << "Arrival frames: " << handleMap(arrivalFramesWithProbabilities);
        out << "\nAction frames: " << handleMap(actionFramesWithProbabilities);
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
