#pragma once

#include <map>
#include <sstream>
#include <iomanip>

namespace LogFormattingUtil
{
    template <typename T>
    std::string formatProbabilityMap(const std::map<T, double> &map, int n = 3)
    {
        // Sort by descending probability
        std::vector<std::pair<T, double>> values(map.begin(), map.end());
        std::sort(values.begin(), values.end(), [&](const std::pair<T, double> &a, const std::pair<T, double> &b) -> bool
        {
            return a.second > b.second;
        });

        // Output the top three
        std::ostringstream out;
        out << std::fixed << std::setprecision(2);
        std::string sep;
        for (auto it = values.begin(); it != values.end() && std::distance(values.begin(), it) < n; it++)
        {
            out << sep << it->first << " (" << (it->second * 100.0) << "%)";
            sep = ", ";
        }
        return out.str();
    }

    template <typename T>
    std::string formatVectorlike(const T &vector)
    {
        std::ostringstream out;
        out << "[";
        std::string sep;
        for (auto val : vector)
        {
            out << sep << val;
            sep = ",";
        }
        out << "]";
        return out.str();
    }
}
