#include "PositionObservationMetadata.h"

#include "CsvTools.h"
#include "Units.h"

namespace WorkerMiningOptimization
{
    int ResendPositionObservations::mostCommonArrivalDelay() const
    {
        if (arrivalDelayAndOccurrences.empty()) return INT_MAX;
        if (arrivalDelayAndOccurrences.size() == 1) return arrivalDelayAndOccurrences.begin()->first;

        int best = -1;
        int bestCount = 0;
        for (const auto &[arrivalDelay, occurrences] : arrivalDelayAndOccurrences)
        {
            if (occurrences > bestCount)
            {
                best = arrivalDelay;
                bestCount = occurrences;
            }
        }

        return best;
    }

    double ResendPositionObservations::expectedMiningDelay(bool otherWorkerAssigned, int commandFrame) const
    {
        if (arrivalDelayAndOccurrences.empty()) return 100.0;

        auto arrivalDelayToMiningDelay = [&](int arrivalDelay)
        {
            // If the worker doesn't arrive at the patch on time, and another worker is assigned to the patch, it is likely that
            // our worker will try to switch patches when its order kicks in, resulting in a full command cycle of extra delay
            if (arrivalDelay > 0 && otherWorkerAssigned)
            {
                return (double)(arrivalDelay + 11 + BWAPI::Broodwar->getLatencyFrames());
            }

            // Compute the delay between the gather command kicking in and mining starting
            // If the worker arrives at the patch on time, the delay is 0
            // If not, the delay will correspond to how long it takes the worker's order process timer to reach 0 again
            int miningDelay;
            if (arrivalDelay <= 0)
            {
                miningDelay = 0;
            }
            else
            {
                miningDelay = arrivalDelay;
                if (miningDelay % 9 != 0) miningDelay += (9 - miningDelay % 9);
            }

            // Check for order process timer resets that will affect start of mining
            int framesToNextReset = OrderProcessTimer::framesToNextReset(commandFrame + BWAPI::Broodwar->getLatencyFrames() + 1);
            if (framesToNextReset < (11 + arrivalDelay))
            {
                // A reset will happen before the worker arrives at the patch
                // On average we will need to wait 4 frames after arrival before mining
                return arrivalDelay + 4.0;
            }
            if (framesToNextReset < (11 + miningDelay))
            {
                // A reset will happen after the worker arrives at the patch, but before it can start mining
                // On average we will need to wait 3.5 frames after the reset
                return framesToNextReset - 11 + 3.5;
            }

            // No reset, return the computed mining delay
            return (double)miningDelay;
        };

        if (arrivalDelayAndOccurrences.size() == 1) return arrivalDelayToMiningDelay(arrivalDelayAndOccurrences.begin()->first);

        // If the most common arrival delay is positive, return it
        auto mostCommon = mostCommonArrivalDelay();
        if (mostCommon > 0) return arrivalDelayToMiningDelay(mostCommon);

        double totalMiningDelay = 0.0;
        int totalOccurrences = 0;
        for (const auto &[arrivalDelay, occurrences] : arrivalDelayAndOccurrences)
        {
            totalMiningDelay += arrivalDelayToMiningDelay(arrivalDelay);
            totalOccurrences += occurrences;
        }

        return totalMiningDelay / (double)totalOccurrences;
    }

    double PositionObservationMetadata::averageDeltaToNormalPathOptimalPosition() const
    {
        if (deltaToNormalPathOptimalPosition.empty()) return 100;
        if (deltaToNormalPathOptimalPosition.size() == 1) return deltaToNormalPathOptimalPosition.begin()->first;

        int accumulator = 0;
        int total = 0;
        for (const auto &[delta, occurrences] : deltaToNormalPathOptimalPosition)
        {
            accumulator += delta * occurrences;
            total += occurrences;
        }

        if (total == 0) return 100;

        return (double)accumulator / (double)total;
    }

    int PositionObservationMetadata::probableDeltaToNormalPathOptimalPosition() const
    {
        if (deltaToNormalPathOptimalPosition.empty()) return 100;
        if (deltaToNormalPathOptimalPosition.size() == 1) return deltaToNormalPathOptimalPosition.begin()->first;

        int best = 100;
        int bestOccurrences = 0;
        for (const auto &[delta, occurrences] : deltaToNormalPathOptimalPosition)
        {
            if (occurrences > bestOccurrences)
            {
                best = delta;
                bestOccurrences = occurrences;
            }
        }

        return best;
    }

    int PositionObservationMetadata::largestDeltaToNormalPathOptimalPosition() const
    {
        if (deltaToNormalPathOptimalPosition.empty()) return 100;
        if (deltaToNormalPathOptimalPosition.size() == 1) return deltaToNormalPathOptimalPosition.begin()->first;

        int best = -1000;
        for (const auto &[delta, occurrences] : deltaToNormalPathOptimalPosition)
        {
            if (delta > best)
            {
                best = delta;
            }
        }

        return best;
    }

    bool PositionObservationMetadata::addObservation(SecondResendPositionObservationMetadata* secondResendPositionData, int arrivalDelta)
    {
        auto &observations = secondResendPositionData ? secondResendPositionData->observations : noSecondResendObservations;
        bool result = observations.empty();
        observations.add(arrivalDelta);
        return result;
    }

    std::vector<const SecondResendPositionObservationMetadata*> PositionObservationMetadata::expectedPathAfterResend() const
    {
        std::vector<const SecondResendPositionObservationMetadata*> result;

        std::map<int, std::vector<const SecondResendPositionObservationMetadata*>> secondResendByDelta;
        for (auto &[secondResendPos, secondResendData] : secondResendMetadata)
        {
            secondResendByDelta[secondResendData.deltaToFirstResend].push_back(&secondResendData);
        }

        const std::unordered_map<PositionAndVelocity, int> *nextOccurrences = &next;
        int delta = 1;
        while (true)
        {
            int bestOccurrences = 0;
            const SecondResendPositionObservationMetadata* bestMetadata = nullptr;
            for (const auto &secondResendData : secondResendByDelta[delta])
            {
                auto occurrencesIt = nextOccurrences->find(secondResendData->pos);
                if (occurrencesIt != nextOccurrences->end() && occurrencesIt->second > bestOccurrences)
                {
                    bestOccurrences = occurrencesIt->second;
                    bestMetadata = secondResendData;
                }
            }

            if (!bestMetadata) break;

            result.push_back(bestMetadata);
            delta++;
            nextOccurrences = &bestMetadata->next;
        }

        return result;
    }

    void PositionObservationMetadata::outputDataFileHeaderRow(std::ofstream &file)
    {
        file << "x;y;path hash;1st resend position;next position(s);no resend collisions;no resend non-collisions;delta to benchmark;"
             << "no 2nd resend arrivals;no 2nd resend collisions;no 2nd resend non-collisions;resend changes path;second resend data\n";
    }

    void PositionObservationMetadata::outputToDataFile(std::ofstream &file, const Resource &resource) const
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

        file << resource->tile.x << ";"
             << resource->tile.y << ";"
             << pathHash << ";"
             << pos << ";";

        outputNext(next);
        file << ";"
             << noResendCollisions << ";"
             << noResendNonCollisions << ";";

        outputOccurrenceMap(deltaToNormalPathOptimalPosition);
        file << ";";

        if ((noSecondResendObservations.collisions
             + noSecondResendObservations.nonCollisions) > 0)
        {
            outputOccurrenceMap(noSecondResendObservations.arrivalDelayAndOccurrences);
        }
        file << ";"
             << noSecondResendObservations.collisions << ";"
             << noSecondResendObservations.nonCollisions << ";"
             << resendChangesPath << ";";

        std::string secondResendPosSep;
        for (const auto &[secondResentPos, secondResendPositionMetadata] : secondResendMetadata)
        {
            file << secondResendPosSep
                 << secondResendPositionMetadata.pos << ":";
            outputNext(secondResendPositionMetadata.next);
            file << ":"
                 << secondResendPositionMetadata.observations.collisions << ":"
                 << secondResendPositionMetadata.observations.nonCollisions << ":"
                 << secondResendPositionMetadata.deltaToFirstResend << ":";
            if ((secondResendPositionMetadata.observations.collisions + secondResendPositionMetadata.observations.nonCollisions) > 0)
            {
                outputOccurrenceMap(secondResendPositionMetadata.observations.arrivalDelayAndOccurrences);
            }
            secondResendPosSep = ",";
        }
        file << ";";

        // TODO: Remove following debugging things once everything is working

        // Best mining delta for most common arrival and collisions
        int bestDelta = 100;
        auto handleObservations = [&](const ResendPositionObservations &observations, int addedDelta)
        {
            if (observations.empty()) return;
            if (deltaToNormalPathOptimalPosition.empty()) return;

            int delta = probableDeltaToNormalPathOptimalPosition() + addedDelta;

            int arrivalDelay = observations.mostCommonArrivalDelay();
            if (arrivalDelay > 0)
            {
                delta += arrivalDelay;
                if (arrivalDelay % 9 != 0) arrivalDelay += (9 - arrivalDelay % 9); // Align to order process timer cycle
            }

            if (observations.collisions > observations.nonCollisions)
            {
                delta += 14;
            }

            if (delta < bestDelta)
            {
                bestDelta = delta;
            }
        };
        std::ostringstream posStr;
        posStr << pos;
        handleObservations(noSecondResendObservations, 0);
        for (const auto &secondResendPosition : expectedPathAfterResend())
        {
            handleObservations(secondResendPosition->observations, secondResendPosition->deltaToFirstResend);
        }
        file << bestDelta << ";";

        // Number of next positions this position has
        file << next.size() << ";";

        // Number of second resend paths this position has
        std::map<int, int> secondResendDeltaOccurrences;
        for (const auto &[_, secondResendPosition] : secondResendMetadata)
        {
            secondResendDeltaOccurrences[secondResendPosition.deltaToFirstResend]++;
        }
        int maxSecondResendDeltaOccurrences = 0;
        for (const auto &[_, occurrences] : secondResendDeltaOccurrences)
        {
            if (occurrences > maxSecondResendDeltaOccurrences) maxSecondResendDeltaOccurrences = occurrences;
        }
        file << maxSecondResendDeltaOccurrences;

        int mostOccurrences = 0;
        int mostOccurrencesPosition = -1;
        int mostOccurrencesDelta = 0;
        int mostOccurrencesCollisions = 0;
        int mostOccurrencesNonCollisions = 0;
        auto countOccurrences = [&](const ResendPositionObservations &observations, int thisPosition)
        {
            for (const auto &[arrivalDelay, occurrences] : observations.arrivalDelayAndOccurrences)
            {
                if (occurrences >= mostOccurrences)
                {
                    mostOccurrences = occurrences;
                    mostOccurrencesPosition = thisPosition;
                    mostOccurrencesDelta = thisPosition + arrivalDelay;
                    mostOccurrencesCollisions = observations.collisions;
                    mostOccurrencesNonCollisions = observations.nonCollisions;
                }
            }

#if INSTRUMENTATION_ENABLED
            if (observations.arrivalDelayAndOccurrences.size() > 1)
            {
                int mostCommonArrivalDelay = observations.mostCommonArrivalDelay();
                int common = 0;
                int uncommon = 0;
                int problematic = 0;
                for (const auto &[arrivalDelay, occurrences] : observations.arrivalDelayAndOccurrences)
                {
                    ((arrivalDelay == mostCommonArrivalDelay) ? common : uncommon) += occurrences;
                    if (mostCommonArrivalDelay <= 0 && arrivalDelay > 0) problematic++;
                }

                if ((common + uncommon) > 10 && (common < uncommon * 4))
                {
                    Log::Get() << "WARNING: Patch " << resource->tile
                               << " position " << (*this) << " : " << thisPosition
                               << " has " << observations.arrivalDelayAndOccurrences.size() << " unstable arrival delays"
                               << "; common(" << mostCommonArrivalDelay << ")=" << common
                               << "; uncommon=" << uncommon
                               << "; problematic=" << problematic;
                }
            }
#endif
        };
        countOccurrences(noSecondResendObservations, 0);
        for (const auto &metadata : secondResendMetadata)
        {
            countOccurrences(metadata.second.observations, metadata.second.deltaToFirstResend);
        }
        if (mostOccurrences > 1)
        {
            file << ";y"
                 << ";" << mostOccurrences
                 << ";" << (probableDeltaToNormalPathOptimalPosition() + mostOccurrencesDelta)
                 << ";" << mostOccurrencesPosition
                 << ";" << mostOccurrencesCollisions
                 << ";" << mostOccurrencesNonCollisions;
        }
        else
        {
            file << ";;;;;;";
        }

        file << "\n";
    }

    bool PositionObservationMetadata::parseFromDataFile(
            const std::vector<std::string> &line,
            std::map<Resource, std::unordered_map<PositionAndVelocity, PositionObservationMetadata>> &map,
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

        auto parseObservations = [&parseOccurrencesMap](
                const std::string &arrivalDelayOccurrences,
                const std::string &collisions,
                const std::string &nonCollisions)
        {
            return ResendPositionObservations{
                    parseOccurrencesMap(arrivalDelayOccurrences),
                    std::stoi(collisions),
                    std::stoi(nonCollisions)
            };
        };

        auto parseSecondResendPositions = [&parseNextPositions, &parseObservations](const std::string &str)
        {
            std::unordered_map<PositionAndVelocity, SecondResendPositionObservationMetadata> result;
            if (str.empty()) return result;

            for (const auto &secondResendData : CsvTools::tokenizeList(str))
            {
                auto data = CsvTools::tokenizeList(secondResendData, ':');
                if (data.size() < 5) continue;
                PositionAndVelocity secondResendPos;
                if (!PositionAndVelocity::tryParse(data[0], secondResendPos)) continue;

                result.emplace(secondResendPos, SecondResendPositionObservationMetadata{
                        secondResendPos,
                        std::stoi(data[4]),
                        parseNextPositions(data[1]),
                        parseObservations((data.size() > 5) ? data[5] : "", data[2], data[3])
                });
            }

            return result;
        };

        if (line.size() < 12) return true;

        BWAPI::TilePosition tile(std::stoi(line[0]), std::stoi(line[1]));
        auto resource = Units::resourceAt(tile);
        if (!resource) return false;

        PositionAndVelocity resendPos;
        if (!PositionAndVelocity::tryParse(line[3], resendPos))
        {
            Log::Get() << "Invalid position string at line " << lineNumber << "; skipping: " << line[2];
            return false;
        }

        auto &resourceMap = map[resource];

        resourceMap.emplace(resendPos, PositionObservationMetadata{
                (uint32_t)std::stoul(line[2]),
                std::move(resendPos),
                parseOccurrencesMap(line[7]),
                parseNextPositions(line[4]),
                parseObservations(line[8], line[9], line[10]),
                parseSecondResendPositions((line.size() > 12) ? line[12] : ""),
                std::stoi(line[5]),
                std::stoi(line[6]),
                std::stoi(line[11])
        });

        return false;
    }

    std::ostream &operator<<(std::ostream &os, const PositionObservationMetadata &optimalGatherPositionMetadata)
    {
        os << optimalGatherPositionMetadata.pos
           << " (d=" << optimalGatherPositionMetadata.probableDeltaToNormalPathOptimalPosition() << ")";

        return os;
    }
}
