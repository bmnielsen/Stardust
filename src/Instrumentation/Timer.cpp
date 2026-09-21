#include "Timer.h"
#include "Common.h"

#include <chrono>
#include <ranges>

#define DEBUG_LOG_EACH_CHECKPOINT false

#if LOGGING_ENABLED
#define TRACK_OVERALL_OBSERVATIONS true
#endif

namespace Timer
{
    namespace
    {
#ifdef DEBUG
        const int LOG_CUTOFF = 10000;
        const int DEBUG_CUTOFF = 10000;
#elif defined(INSTRUMENTATION_ENABLED_VERBOSE)
        const int LOG_CUTOFF = 1000;
        const int DEBUG_CUTOFF = 1000;
#elif defined(INSTRUMENTATION_ENABLED)
        const int LOG_CUTOFF = 100;
        const int DEBUG_CUTOFF = 100;
#else
        const int LOG_CUTOFF = 55;
        const int DEBUG_CUTOFF = 45;
#endif

#if TRACK_OVERALL_OBSERVATIONS
        std::map<std::string, std::vector<long long>> labelToObservations;
#endif

        // Following apply to a specific frame's timing

        std::string overallLabel;
        std::chrono::steady_clock::time_point startPoint;
        std::chrono::steady_clock::time_point lastCheckpoint;
        std::vector<std::pair<std::string, long long>> checkpoints;

        bool sortCheckpoints(std::pair<std::string, long long> &first, std::pair<std::string, long long> &second)
        {
            return first.second > second.second;
        }
    }

    void initialize()
    {
#if TRACK_OVERALL_OBSERVATIONS
        labelToObservations.clear();
#endif
    }

    void gameEnd()
    {
#if TRACK_OVERALL_OBSERVATIONS
        std::vector<std::tuple<std::string, double, long long, long long, long long, long long, long long>> data;

        for (auto &[label, observations] : labelToObservations)
        {
            if (observations.empty()) continue;

            std::sort(observations.begin(), observations.end());

            data.emplace_back(
                label,
                (double)std::reduce(observations.begin(), observations.end()) / (double)observations.size(),
                observations[(observations.size() * 999) / 1000],
                observations[(observations.size() * 99) / 100],
                observations[(observations.size() * 95) / 100],
                observations[(observations.size() * 90) / 100],
                observations[(observations.size() * 50) / 100]);
        }

        std::sort(data.begin(), data.end(), [](auto &a, auto &b)
        {
            return std::get<3>(a) > std::get<3>(b);
        });

        for (const auto &[label, avg, p999, p99, p95, p90, p50]
                : data | std::views::take(11))
        {
            Log::Get() << std::fixed << std::setprecision(2) << std::setw(35) << label
                       << ": avg=" << std::left << std::setw(8) << avg
                       << "p99.9=" << std::left << std::setw(6) << p999
                       << "p99=" << std::left << std::setw(6) << p99
                       << "p95=" << std::left << std::setw(6) << p95
                       << "p90=" << std::left << std::setw(6) << p90
                       << "p50=" << std::left << std::setw(6) << p50;
        }
#endif
    }

    void start(const std::string &label)
    {
        overallLabel = label;
        checkpoints.clear();
        startPoint = lastCheckpoint = std::chrono::high_resolution_clock::now();
    }

    void checkpoint(const std::string &label)
    {
#if DEBUG_LOG_EACH_CHECKPOINT
        Log::Debug() << label;
#endif
        auto now = std::chrono::high_resolution_clock::now();
        auto checkpointDuration = std::chrono::duration_cast<std::chrono::microseconds>(now - lastCheckpoint).count();
        checkpoints.emplace_back(label, checkpointDuration);
        lastCheckpoint = now;

#if TRACK_OVERALL_OBSERVATIONS
        if (currentFrame > 0) labelToObservations[label].push_back(checkpointDuration);
#endif
    }

    void stop(bool forceOutput)
    {
        long long overall = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startPoint).count();

#if TRACK_OVERALL_OBSERVATIONS
        if (currentFrame > 0)
        {
            labelToObservations[overallLabel].push_back(
                std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startPoint).count());
        }
#endif

        if (forceOutput || overall > DEBUG_CUTOFF)
        {
            std::ostringstream msg;
            msg << overallLabel << " took " << overall << "ms";

            std::sort(checkpoints.begin(), checkpoints.end(), sortCheckpoints);

            bool first = true;
            for (auto &checkpoint : checkpoints)
            {
                if (checkpoint.second < (DEBUG_CUTOFF / 5)) continue;

                if (first)
                {
                    msg << ", longest: ";
                    first = false;
                }
                else
                    msg << ", ";

                msg << checkpoint.first << ": " << checkpoint.second << "us";
            }

            Log::Debug() << msg.str();

            if (forceOutput || overall > LOG_CUTOFF)
                Log::Get() << msg.str();
        }
    }
}
