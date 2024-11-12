#include "ReturnPositionObservations.h"

#include "WorkerMiningOptimization.h"
#include "CsvTools.h"
#include "Units.h"
#include "DebugFlag_WorkerMiningOptimization.h"

namespace WorkerMiningOptimization
{
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

        // TODO
        return 0;
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
        file << "x;y;path hash;position;no resend next position(s);no resend arrival(s);no resend collisions;no resend non collisions;"
             << "no resend lost speed;no resend kept speed;resend arrival(s);resend collisions;resend non collisions;resend lost speed;"
             << "resend kept speed;arrival delta\n";
#else
        file << "x;y;path hash;position;no resend next position(s);no resend arrival(s);no resend collisions;no resend non collisions;"
             << "no resend lost speed;no resend kept speed;resend arrival(s);resend collisions;resend non collisions;resend lost speed;"
             << "resend kept speed\n";
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
        auto outputArrivalObservations = [&](const ReturnArrivalObservations &observations)
        {
            file << ";";
            outputOccurrenceMap(observations.arrivalDelayAndOccurrences);
            file << ";" << observations.collisions
                 << ";" << observations.noncollisions
                 << ";" << observations.lostSpeed
                 << ";" << observations.keptSpeed;
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

        auto parseArrivalObservations = [&](
                const std::string &arrivalDelayOccurrences,
                const std::string &collisions,
                const std::string &noncollisions,
                const std::string &lostSpeed,
                const std::string &keptSpeed)
        {
            return ReturnArrivalObservations{
                    parseOccurrencesMap(arrivalDelayOccurrences),
                    std::stoi(collisions),
                    std::stoi(noncollisions),
                    std::stoi(lostSpeed),
                    std::stoi(keptSpeed)
            };
        };

        if (line.size() < 14) return true;

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
                parseArrivalObservations(line[5], line[6], line[7], line[8], line[9]),
                parseArrivalObservations(line[10], line[11], line[12], line[13], (line.size() > 14) ? line[14] : "")
        });

        return false;
    }

    std::ostream &operator<<(std::ostream &os, const ReturnPositionObservations &optimalGatherPositionMetadata)
    {
        os << optimalGatherPositionMetadata.pos;
        return os;
    }
}
