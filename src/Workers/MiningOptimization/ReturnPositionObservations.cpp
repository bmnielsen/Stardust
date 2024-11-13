#include "ReturnPositionObservations.h"

#include "WorkerMiningOptimization.h"
#include "CsvTools.h"
#include "Units.h"
#include "OrderProcessTimer.h"
#include "DebugFlag_WorkerMiningOptimization.h"

namespace WorkerMiningOptimization
{
    double ReturnSpeedOccurrences::expectedDeltaToNormal() const
    {
        // Using the following logic:
        // - Low exit speed is the norm, so does not affect the result
        // - High exit speed saves 4 frames
        // - Medium exit speed saves 2 frames
        // - Collisions cost 14 frames
        int total = collision + lowExitSpeed + mediumExitSpeed + highExitSpeed;
        if (total == 0) return 0.0;

        return (double)((collision * 14) - (mediumExitSpeed * 2) - (highExitSpeed * 4)) / (double)total;
    }

    int ReturnArrivalObservations::largestArrivalDelay() const
    {
        if (arrivalDelayAndOccurrences.empty()) return 100;
        if (arrivalDelayAndOccurrences.size() == 1) return arrivalDelayAndOccurrences.begin()->first;

        int best = -1000;
        for (const auto &[delta, occurrences] : arrivalDelayAndOccurrences)
        {
            if (delta > best)
            {
                best = delta;
            }
        }

        return best;
    }

    double ReturnArrivalObservations::expectedDeliveryDelay(int commandFrame) const
    {
        if (arrivalDelayAndOccurrences.empty()) return 100.0;

        if (arrivalDelayAndOccurrences.size() == 1)
        {
            return deliveryDelayForArrivalFrame(arrivalDelayAndOccurrences.begin()->first, 0, commandFrame + BWAPI::Broodwar->getLatencyFrames());
        }

        double totalDelay = 0.0;
        int totalOccurrences = 0;
        for (const auto &[arrivalDelay, occurrences] : arrivalDelayAndOccurrences)
        {
            totalDelay += deliveryDelayForArrivalFrame(arrivalDelay, 0, commandFrame + BWAPI::Broodwar->getLatencyFrames());
            totalOccurrences += occurrences;
        }

        return totalDelay / (double)totalOccurrences;
    }

    double ReturnArrivalObservations::expectedNoResendDeliveryDelay(const MyWorker &worker) const
    {
        if (arrivalDelayAndOccurrences.empty()) return 100.0;

        if (arrivalDelayAndOccurrences.size() == 1)
        {
            return deliveryDelayForArrivalFrame(arrivalDelayAndOccurrences.begin()->first,
                                                worker->orderProcessTimer,
                                                BWAPI::Broodwar->getFrameCount());
        }

        double totalDelay = 0.0;
        int totalOccurrences = 0;
        for (const auto &[arrivalDelay, occurrences] : arrivalDelayAndOccurrences)
        {
            totalDelay += deliveryDelayForArrivalFrame(arrivalDelay, worker->orderProcessTimer, BWAPI::Broodwar->getFrameCount());
            totalOccurrences += occurrences;
        }

        return totalDelay / (double)totalOccurrences;
    }

    double ReturnArrivalObservations::deliveryDelayForArrivalFrame(int arrivalFrame,
                                                                   int knownOrderProcessTimer,
                                                                   int knownOrderProcessTimerFrame) const
    {
        // Compute the order process timer at arrival
        // This takes order process timer resets into account
        int orderProcessTimerAtArrival = OrderProcessTimer::unitOrderProcessTimerAtDelta(
                knownOrderProcessTimerFrame, knownOrderProcessTimer, arrivalFrame - knownOrderProcessTimerFrame);

        // If we don't know what the order process timer will be at arrival, compute an average delay considering the different exit timings
        // depending on whether the delivery happens at arrival or not
        if (orderProcessTimerAtArrival == -1)
        {
            // If the arrival frame is a reset frame, this changes the math slightly as there are only 8 possible reset values
            if (OrderProcessTimer::isResetFrame(arrivalFrame))
            {
                return (deliveryAtArrivalSpeeds.expectedDeltaToNormal() + 28.0 + (7.0 * deliveryAfterArrivalSpeeds.expectedDeltaToNormal())) / 8.0;
            }
            return (deliveryAtArrivalSpeeds.expectedDeltaToNormal() + 36.0 + (8.0 * deliveryAfterArrivalSpeeds.expectedDeltaToNormal())) / 9.0;
        }

        // Compute the expected delivery frame if no order process timer reset occurs
        int deliveryFrame = arrivalFrame + orderProcessTimerAtArrival;

        // Handle the case where the order process timer resets between arrival and expected delivery
        int nextResetFrame = OrderProcessTimer::nextResetFrame(arrivalFrame);
        if (nextResetFrame <= deliveryFrame)
        {
            // Average will be 3.5 frames of delay after the reset
            return (nextResetFrame - arrivalFrame) + 3.5 + deliveryAfterArrivalSpeeds.expectedDeltaToNormal();
        }

        // No order process timer reset affects the timing, so just return the appropriate value depending on whether we deliver on the arrival frame
        // or not
        if (deliveryFrame == arrivalFrame)
        {
            return deliveryAtArrivalSpeeds.expectedDeltaToNormal();
        }
        return (deliveryFrame - arrivalFrame) + deliveryAfterArrivalSpeeds.expectedDeltaToNormal();
    }

    bool ReturnPositionObservations::suitableForExploration() const
    {
        if (!resendArrivalObservations.empty()) return false; // Have already explored

        // Don't explore if any of the observed arrival delays are outside our exploration horizon
        int referenceFrame = 8 + BWAPI::Broodwar->getLatencyFrames();
        for (const auto &[arrivalDelay, _] : noResendArrivalObservations.arrivalDelayAndOccurrences)
        {
            if (arrivalDelay > (referenceFrame + RETURN_EXPLORE_BEFORE) || arrivalDelay < (referenceFrame - RETURN_EXPLORE_AFTER))
            {
                return false;
            }
        }

        return true;
    }

    void ReturnPositionObservations::outputDataFileHeaderRow(std::ofstream &file)
    {
#if DATAFILE_RETURN_DEBUGCOLUMNS
        file << "x;y;path hash;position;no resend next position(s);no resend arrival(s);no resend speeds delivery after arrival;"
             << "no resend speeds delivery at arrival;resend arrival(s);resend speeds delivery after arrival;resend speeds delivery at arrival;"
             << "arrival delta\n";
#else
        file << "x;y;path hash;position;no resend next position(s);no resend arrival(s);no resend speeds delivery after arrival;"
             << "no resend speeds delivery at arrival;resend arrival(s);resend speeds delivery after arrival;resend speeds delivery at arrival\n"
#endif
    }

    void ReturnPositionObservations::outputToDataFile(std::ofstream &file, const Resource &resource) const
    {
        auto outputNext = [&file](const std::unordered_map<PositionAndVelocity, int> &nextPositions)
        {
            std::string nextPosSep;
            for (const auto &[nextPos, nextOccurrences] : nextPositions)
            {
                file << nextPosSep << nextPos << "|" << nextOccurrences;
                nextPosSep = "_";
            }
        };
        auto outputOccurrenceMap = [&file](const std::unordered_map<int, int> &occurrenceMap)
        {
            std::string sep;
            for (const auto &[data, occurrences] : occurrenceMap)
            {
                file << sep << data << "|" << occurrences;
                sep = "_";
            }
        };
        auto outputSpeeds = [&file](const ReturnSpeedOccurrences &speeds)
        {
            file << ";" << speeds.collision
                 << "|" << speeds.lowExitSpeed
                 << "|" << speeds.mediumExitSpeed
                 << "|" << speeds.highExitSpeed;
        };
        auto outputArrivalObservations = [&](const ReturnArrivalObservations &observations)
        {
            file << ";";
            outputOccurrenceMap(observations.arrivalDelayAndOccurrences);
            outputSpeeds(observations.deliveryAfterArrivalSpeeds);
            outputSpeeds(observations.deliveryAtArrivalSpeeds);
        };

        file << resource->tile.x << ";"
             << resource->tile.y << ";"
             << pathHash << ";"
             << pos << ";";
        outputNext(nextPositionAndOccurrences);
        outputArrivalObservations(noResendArrivalObservations);
        outputArrivalObservations(resendArrivalObservations);

#if DATAFILE_RETURN_DEBUGCOLUMNS
        // Output a column with the difference in arrival between sending and non-sending
        file << ";";
        if (noResendArrivalObservations.arrivalDelayAndOccurrences.size() == 1 &&
            resendArrivalObservations.arrivalDelayAndOccurrences.size() == 1)
        {
            file << (resendArrivalObservations.arrivalDelayAndOccurrences.begin()->first -
                     noResendArrivalObservations.arrivalDelayAndOccurrences.begin()->first);
        }
#endif

        file << "\n";
    }

    bool ReturnPositionObservations::parseFromDataFile(
            const std::vector<std::string> &line,
            std::map<Resource, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &map,
            int lineNumber)
    {
        auto parseNextPositions = [](const std::string &str)
        {
            std::unordered_map<PositionAndVelocity, int> result;

            for (const auto &observations : CsvTools::tokenizeList(str, '_'))
            {
                auto data = CsvTools::tokenizeList(observations, '|');
                if (data.size() < 2) continue;
                PositionAndVelocity nextPos;
                if (!PositionAndVelocity::tryParse(data[0], nextPos)) continue;

                result.emplace(nextPos, std::stoi(data[1]));
            }

            return result;
        };

        auto parseOccurrencesMap = [](const std::string &occurrencesMap)
        {
            std::unordered_map<int, int> result;

            if (!occurrencesMap.empty())
            {
                for (const auto &observations : CsvTools::tokenizeList(occurrencesMap, '_'))
                {
                    auto data = CsvTools::tokenizeList(observations, '|');
                    if (data.size() < 2) continue;

                    result.emplace(std::stoi(data[0]), std::stoi(data[1]));
                }
            }

            return result;
        };

        auto parseSpeeds = [](const std::string &speeds)
        {
            auto data = CsvTools::tokenizeList(speeds, '|');
            if (data.size() != 4) return ReturnSpeedOccurrences{0, 0, 0, 0};
            return ReturnSpeedOccurrences{
                std::stoi(data[0]),
                std::stoi(data[1]),
                std::stoi(data[2]),
                std::stoi(data[3])
            };
        };

        auto parseArrivalObservations = [&](
                const std::string &arrivalDelayOccurrences,
                const std::string &deliveryAfterArrivalSpeeds,
                const std::string &deliveryAtArrivalSpeeds)
        {
            return ReturnArrivalObservations{
                    parseOccurrencesMap(arrivalDelayOccurrences),
                    parseSpeeds(deliveryAfterArrivalSpeeds),
                    parseSpeeds(deliveryAtArrivalSpeeds)
            };
        };

        if (line.size() < 11) return true;

        BWAPI::TilePosition tile(std::stoi(line[0]), std::stoi(line[1]));
        auto resource = Units::resourceAt(tile);
        if (!resource) return false;

        PositionAndVelocity pos;
        if (!PositionAndVelocity::tryParse(line[3], pos))
        {
            Log::Get() << "Invalid position string at line " << lineNumber << "; skipping: " << line[2];
            return false;
        }

        auto &resourceMap = map[resource];

        resourceMap.emplace(pos, ReturnPositionObservations{
                (uint32_t)std::stoul(line[2]),
                pos,
                parseNextPositions(line[4]),
                parseArrivalObservations(line[5], line[6], line[7]),
                parseArrivalObservations(line[8], line[9], line[10])
        });

        return false;
    }

    std::ostream &operator<<(std::ostream &os, const ReturnPositionObservations &optimalGatherPositionMetadata)
    {
        os << optimalGatherPositionMetadata.pos;
        return os;
    }
}
