#include "WorkerMiningOptimization.h"

#include <utility>

#include "Workers.h"
#include "OrderProcessTimer.h"
#include "PositionAndVelocity.h"
#include "FileTools.h"
#include "CsvTools.h"
#include "Units.h"
#include "Geo.h"

#if INSTRUMENTATION_ENABLED
#define TAKEOVER_DEBUG true
#define OPTIMALPOSITIONS_DEBUG true
#endif

#define SAVED_POSITIONS_ONLY false

namespace WorkerMiningOptimization
{
    namespace
    {
        // This is the structure we use to track observed positions and our track record using them
        struct PositionObservationMetadata
        {
        public:
            PositionAndVelocity pos;

            // How many times we have observed this position with no resends potentially disturbing the path
            unsigned int observations = 0;

            // How many times this position was used successfully
            unsigned int successes = 0;

            // How many times this position was used unsuccessfully
            unsigned int failures = 0;

            // Additional position metadata gathered from the failure cases
            std::map<PositionAndVelocity, std::map<int, PositionObservationMetadata>> failurePositionMetadata;

            void trackObservation()
            {
                if (atObservationCap()) return;
                observations++;
            }

            void trackSuccess()
            {
                if (atObservationCap()) return;
                successes++;
            }

            void trackFailure()
            {
                if (atObservationCap()) return;
                failures++;
            }

            // Tracks a nonoptimal resend for the normal approach timing optimization (where we expect to reach the patch 11+LF frames after resend)
            void trackNonoptimalResend(const PositionAndVelocity &optimalPosition,
                                       const std::shared_ptr<PositionAndVelocity> &secondResendPosition,
                                       int resentDelta,
                                       int secondResentDelta)
            {
                if (atObservationCap()) return;

                // A special case is if the worker actually arrived at the patch faster because of the resend
                // This doesn't help us, since the worker has to wait for the order process timer to reach 0, but
                // we get the same performance as we expected (as if the path hadn't changed), so we can treat it as
                // a success
                if (resentDelta >= 0)
                {
                    successes++;
                    return;
                }

                failures++;

                auto &optimalPositionMetadata = failurePositionMetadata[optimalPosition][-resentDelta];
                if (!optimalPositionMetadata.atObservationCap())
                {
                    if (!secondResendPosition)
                    {
                        optimalPositionMetadata.trackObservation();
                    }
                    else if (secondResentDelta == 0)
                    {
                        optimalPositionMetadata.trackSuccess();
                    }
                }

                if (secondResendPosition && secondResentDelta != 0)
                {
                    auto &secondResendPositionMetadata = failurePositionMetadata[*secondResendPosition][-resentDelta];
                    if (!secondResendPositionMetadata.atObservationCap())
                    {
                        (secondResentDelta > 0 ? secondResendPositionMetadata.successes : secondResendPositionMetadata.failures)++;
                    }
                }
            }

        private:
            [[nodiscard]] bool atObservationCap() const
            {
                // Set a cap on how many observations we track to reduce computation time
                // Beyond a certain point additional observations are not going to have any impact anyway
                return (observations + successes + failures) >= 1000;
            }
        };

        std::ostream &operator<<(std::ostream &os, const PositionObservationMetadata &optimalGatherPositionMetadata)
        {
            os << optimalGatherPositionMetadata.pos
               << " (o=" << optimalGatherPositionMetadata.observations
               << " s=" << optimalGatherPositionMetadata.successes
               << " f=" << optimalGatherPositionMetadata.failures
               << ")";

            return os;
        }

        unsigned int expectedGatherDelay(const PositionObservationMetadata &metadata)
        {
            // Gather the aggregated delays across all nonoptimal resends
            unsigned int count = metadata.successes;
            unsigned int delays = 0;
            for (const auto &[_, delayAndMetadata] : metadata.failurePositionMetadata)
            {
                for (const auto &[delay, secondMetadata] : delayAndMetadata)
                {
                    count += secondMetadata.successes + secondMetadata.failures;

                    // If the delay is exactly LF after, the unit will be busy and this will incur an extra frame of delay
                    int effectiveDelay = delay + ((delay == BWAPI::Broodwar->getFrameCount()) ? 1 : 0);
                    delays += secondMetadata.successes * effectiveDelay + secondMetadata.failures * (effectiveDelay + 5);
                }
            }

            if (count == 0) return 0;

            // Integer division with ceiling
            return (delays + count - 1) / count;
        }

        bool shouldResendGatherCommand(const MyWorker &worker,
                                       const PositionObservationMetadata &positionMetadata,
                                       unsigned int &expectedDelay)
        {
            expectedDelay = expectedGatherDelay(positionMetadata);
            if (expectedDelay == 0) return true;

            // If we can predict the order timer value at arrival, check if it is better or worse than the observed results on this patch
            if (worker->orderProcessTimer != -1)
            {
                int orderProcessTimerAtArrival = worker->orderProcessTimer - BWAPI::Broodwar->getLatencyFrames() - 11 + 1;
                while (orderProcessTimerAtArrival < 0)
                {
                    orderProcessTimerAtArrival += 9;
                }

#if OPTIMALPOSITIONS_DEBUG
                if (expectedDelay >= orderProcessTimerAtArrival)
                {
                    CherryVis::log(worker->id) << "Not resending at " << positionMetadata << " as expected delay " << expectedDelay
                                               << " is no better than expected order process timer at arrival " << orderProcessTimerAtArrival;
                }
#endif

                return expectedDelay < orderProcessTimerAtArrival;
            }

            // The order timer will be randomized at arrival, so resend if the metadata indicates we on average would benefit
#if OPTIMALPOSITIONS_DEBUG
            if (expectedDelay >= 5)
            {
                CherryVis::log(worker->id) << "Not resending at " << positionMetadata << " as expected delay " << expectedDelay
                                           << " is no better than average delay";
            }
#endif

            return expectedDelay < 5;
        }

        bool shouldResendGatherCommand(const MyWorker &worker,
                                       const std::map<int, PositionObservationMetadata> &positionMetadata,
                                       bool &mayGetUnitBusy)
        {
            // Sum up the performance of this position
            unsigned int successes = 0;
            unsigned int failures = 0;
            for (const auto &[_, metadata] : positionMetadata)
            {
                successes += metadata.successes;
                failures += metadata.failures;
            }

            mayGetUnitBusy = positionMetadata.contains(BWAPI::Broodwar->getLatencyFrames()) &&
                    !positionMetadata.contains(BWAPI::Broodwar->getLatencyFrames() + 1);

            // TODO: Maybe tune this if needed
            return successes >= failures;
        }

        struct WorkerGatherStatus
        {
            // The worker gathering
            MyWorker worker;

            // The resource being gathered from
            Resource resource;

            // The last frame this worker was optimized
            int lastProcessedFrame;

            // The positions this worker has visited while on its way to the patch
            std::vector<std::shared_ptr<PositionAndVelocity>> positionHistory;

            // The position at which the gather command was resent, or nullptr if it hasn't been resent
            std::shared_ptr<PositionAndVelocity> resentPosition;

            // The position at which the gather command was resent again, or nullptr if it hasn't been resent
            std::shared_ptr<PositionAndVelocity> secondResentPosition;

            // Used to mark that the worker should have the gather command resent on this specific frame
            int resendCommandOnFrame;

            // Tracks whether the worker has passed the position LF+1 before reaching 10 distance from the patch
            std::shared_ptr<PositionAndVelocity> passed10DistancePosition;

            // Mode to use for takeover, current allowed values: 0=use normal approach optimization, 1=use takeover optimization, 2=at patch
            int takeoverMode;

            WorkerGatherStatus(MyWorker worker, Resource resource)
                    : worker(std::move(worker))
                    , resource(std::move(resource))
                    , lastProcessedFrame(-2)
                    , resendCommandOnFrame(-2)
                    , takeoverMode(0)
            {}

            void reset()
            {
                lastProcessedFrame = -2;
                positionHistory.clear();
                resentPosition = nullptr;
                secondResentPosition = nullptr;
                resendCommandOnFrame = -2;
                passed10DistancePosition = nullptr;
                takeoverMode = 0;
            }

            [[nodiscard]] bool resentOnSchedule() const
            {
                return resendCommandOnFrame != -2;
            }
        };

        // Metadata for positions where we can resend the gather command to start mining immediately on arrival
        std::map<Resource, std::map<PositionAndVelocity, PositionObservationMetadata>> resourceToOptimalGatherPositions;

        // Metadata for positions LF+1 frames before reaching 10 or less distance from the patch
        // Workers can try to switch to another patch if their chosen patch is being mined once they reach this distance, so we use these
        // positions to detect when we need to start resending gather commands to ensure mineral locking
        std::map<Resource, std::map<PositionAndVelocity, PositionObservationMetadata>> resourceTo10DistancePositions;

        // Metadata for positions where we can resend the gather command to optimize the takeover frame and reach the patch on time
        std::map<Resource, std::map<PositionAndVelocity, PositionObservationMetadata>> resourceToTakeoverResendPositions;

        std::map<MyWorker, WorkerGatherStatus> workerGatherStatuses;

        std::string optimalGatherPositionsFilename(bool writing = false)
        {
            auto filename = std::ostringstream()
                    << "gatherpositions_" << BWAPI::Broodwar->mapHash()
                    << "_lf" << BWAPI::Broodwar->getLatencyFrames();
            return FileTools::getFilePath(filename.str(), "csv", writing);
        }

        std::string tenDistancePositionsFilename(bool writing = false)
        {
            auto filename = std::ostringstream()
                    << "10distance_" << BWAPI::Broodwar->mapHash()
                    << "_lf" << BWAPI::Broodwar->getLatencyFrames();
            return FileTools::getFilePath(filename.str(), "csv", writing);
        }

        std::string takeoverResendPositionsFilename(bool writing = false)
        {
            auto filename = std::ostringstream()
                    << "takeoverpositions_" << BWAPI::Broodwar->mapHash()
                    << "_lf" << BWAPI::Broodwar->getLatencyFrames();
            return FileTools::getFilePath(filename.str(), "csv", writing);
        }

        void parsePositionObservationMetadataFile(const std::string &filename,
                                                  std::map<Resource, std::map<PositionAndVelocity, PositionObservationMetadata>> &map)
        {
            map.clear();

            std::ifstream file;
            file.open(filename);
            if (!file.good()) return;

            int lineNumber = 0;
            try
            {
                // Read and parse each position
                int count = 0;
                while (true)
                {
                    lineNumber++;

                    auto line = CsvTools::readNextLine(file);
                    if (line.size() < 6) break;

                    BWAPI::TilePosition tile(std::stoi(line[0]), std::stoi(line[1]));
                    auto resource = Units::resourceAt(tile);
                    if (!resource) continue;

                    if (!PositionAndVelocity::isValidString(line[2]))
                    {
                        Log::Get() << "Invalid position string at line " << lineNumber << "; skipping: " << line[2];
                        continue;
                    }
                    auto pos = PositionAndVelocity::fromString(line[2]);

                    std::map<PositionAndVelocity, std::map<int, PositionObservationMetadata>> failurePositionMetadata;
                    if (line.size() > 6)
                    {
                        for (const auto &nonoptimalResendOptimalPosition : CsvTools::tokenizeList(line[6]))
                        {
                            auto data = CsvTools::tokenizeList(nonoptimalResendOptimalPosition, ':');
                            if (data.size() < 5) continue;
                            if (!PositionAndVelocity::isValidString(data[0])) continue;
                            auto nonoptimalPos = PositionAndVelocity::fromString(data[0]);

                            failurePositionMetadata[nonoptimalPos].emplace(std::stoi(data[1]), PositionObservationMetadata{
                                    nonoptimalPos,
                                    (unsigned int)std::stoi(data[2]),
                                    (unsigned int)std::stoi(data[3]),
                                    (unsigned int)std::stoi(data[4])});
                        }
                    }

                    map[resource].emplace(
                            pos,
                            PositionObservationMetadata{
                                    pos,
                                    (unsigned int)std::stoi(line[3]),
                                    (unsigned int)std::stoi(line[4]),
                                    (unsigned int)std::stoi(line[5]),
                                    failurePositionMetadata});
                    count++;
                }

                Log::Get() << "Read " << count << " positions metadata from " << filename;
            }
            catch (std::exception &ex)
            {
                Log::Get() << "Exception caught attempting to read positions metadata from " << filename
                           << " at line " << lineNumber << ": " << ex.what();
            }
        }

        void writePositionObservationMetadataFile(const std::string &filename,
                                                  std::map<Resource, std::map<PositionAndVelocity, PositionObservationMetadata>> &map)
        {
            std::ofstream file;
            file.open(filename, std::ofstream::trunc);

            int count = 0;
            for (auto &resourceAndOptimalGatherPositions : map)
            {
                for (auto &optimalOrderPosition : resourceAndOptimalGatherPositions.second)
                {
                    file << resourceAndOptimalGatherPositions.first->tile.x << ";"
                         << resourceAndOptimalGatherPositions.first->tile.y << ";"
                         << optimalOrderPosition.first << ";"
                         << optimalOrderPosition.second.observations << ";"
                         << optimalOrderPosition.second.successes << ";"
                         << optimalOrderPosition.second.failures << ";";
                    std::string sep;
                    for (const auto &[pos, deltaAndMetadata] : optimalOrderPosition.second.failurePositionMetadata)
                    {
                        for (const auto &[delta, metadata] : deltaAndMetadata)
                        {
                            file << sep << pos << ":" << delta << ":"
                                 << metadata.observations << ":"
                                 << metadata.successes << ":"
                                 << metadata.failures;
                        }
                        sep = ",";
                    }
                    file << "\n";
                    count++;
                }
            }

            file.close();
            Log::Get() << "Wrote " << count << " positions metadata to " << filename;
        }

        void handleObservation(const WorkerGatherStatus &workerStatus,
                               const std::shared_ptr<PositionAndVelocity> &observedPosition,
                               const std::shared_ptr<PositionAndVelocity> &resentPosition,
                               std::map<PositionAndVelocity, PositionObservationMetadata> &observations,
                               bool tenDistance = false)
        {
            if (workerStatus.resentOnSchedule()) return;

            // If no command was resent, we can track an observation
            // We don't track observations otherwise since resending the command can change the path
            if (!resentPosition)
            {
                auto metadata = observations.find(*observedPosition);
                if (metadata == observations.end())
                {
                    metadata = observations.emplace(*observedPosition, PositionObservationMetadata{*observedPosition}).first;
                }
                metadata->second.trackObservation();

#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(workerStatus.worker->id) << "Tracking observation on " << (tenDistance ? "10-distance " : "") << metadata->second;
                CherryVis::log(workerStatus.resource->id) << "Tracking observation on " << (tenDistance ? "10-distance " : "") << metadata->second;
#endif

                return;
            }

            // If a command was resent, try to find its metadata
            auto resendPositionData = observations.find(*resentPosition);
            if (resendPositionData == observations.end()) return;

            // Find the delta between the observed and resent positions in the positions history, which we use to measure
            // the risk/reward of using a position
            auto getDelta = [&](const std::shared_ptr<PositionAndVelocity> &position)
            {
                if (!position) return LONG_MAX;

                auto observedIt = workerStatus.positionHistory.rend();
                auto posIt = workerStatus.positionHistory.rend();
                for (auto it = workerStatus.positionHistory.rbegin(); it != workerStatus.positionHistory.rend(); it++)
                {
                    if ((*it) == observedPosition) observedIt = it;
                    if ((*it) == position) posIt = it;
                    if (observedIt != workerStatus.positionHistory.rend() && posIt != workerStatus.positionHistory.rend()) break;
                }
                if (observedIt != workerStatus.positionHistory.rend() && posIt != workerStatus.positionHistory.rend())
                {
                    return std::distance(posIt, observedIt);
                }

                return LONG_MAX;
            };

            auto resentDelta = getDelta(workerStatus.resentPosition);
            if (resentDelta == LONG_MAX) return;

            if (tenDistance)
            {
                // It is OK if we sent the order too early, as this does not introduce a risk of losing mineral locking
                // Otherwise we just mark it as a failure without including any additional metadata right now
                if (resentDelta <= 1)
                {
                    resendPositionData->second.successes++;

#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(workerStatus.worker->id) << "Tracking success on 10-distance " << resendPositionData->second;
                    CherryVis::log(workerStatus.resource->id) << "Tracking success on 10-distance " << resendPositionData->second;
#endif
                }
                else
                {
                    resendPositionData->second.failures++;

#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(workerStatus.worker->id) << "Tracking failure on 10-distance " << resendPositionData->second;
                    CherryVis::log(workerStatus.resource->id) << "Tracking failure on 10-distance " << resendPositionData->second;
#endif
                }
                return;
            }

            // Track the nonoptimal resend for arrival optimization
            auto secondResentDelta = getDelta(workerStatus.secondResentPosition);
            resendPositionData->second.trackNonoptimalResend(*observedPosition,
                                                             workerStatus.secondResentPosition,
                                                             (int)resentDelta,
                                                             (int)secondResentDelta);

#if OPTIMALPOSITIONS_DEBUG
            if (resentDelta >= 0)
            {
                CherryVis::log(workerStatus.worker->id) << "Tracking success on " << resendPositionData->second;
                CherryVis::log(workerStatus.resource->id) << "Tracking success on " << resendPositionData->second;
            }
            else
            {
                CherryVis::log(workerStatus.worker->id) << "Tracking failure on " << resendPositionData->second;
                CherryVis::log(workerStatus.resource->id) << "Tracking failure on " << resendPositionData->second;
            }
#endif
        }

        void updateTakeoverMetadata(WorkerGatherStatus &workerStatus,
                                    const Resource &resource,
                                    std::map<PositionAndVelocity, PositionObservationMetadata> &tenDistancePositions,
                                    std::map<PositionAndVelocity, PositionObservationMetadata> &takeoverResendPositions,
                                    bool switchedPatches = false)
        {
            // Abort if we've already cleared our history
            if (workerStatus.positionHistory.size() < (BWAPI::Broodwar->getLatencyFrames() + 11)) return;

            // Abort if we had to resend a position on a schedule, which might ruin our timings
            if (workerStatus.resentOnSchedule()) return;

            // Track 10-distance if there was no resend for optimizing arrival
            if (!workerStatus.resentPosition)
            {
                // Find the position where we were LF+1 from being 10 distance from the patch
                auto it = workerStatus.positionHistory.rbegin();
                auto beforeIt = it + BWAPI::Broodwar->getLatencyFrames();
                for (; beforeIt != workerStatus.positionHistory.rend();)
                {
                    auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                        (*it)->pos(),
                                                        BWAPI::UnitTypes::Resource_Mineral_Field,
                                                        resource->center);
                    if (dist > 10) break;
                    it++;
                    beforeIt++;
                }
                if (it != workerStatus.positionHistory.rbegin() && beforeIt != workerStatus.positionHistory.rend())
                {
                    handleObservation(workerStatus, *beforeIt, workerStatus.passed10DistancePosition, tenDistancePositions, true);
                }
            }

            // If we switched patches, track a failure if we resent
            if (switchedPatches)
            {
                if (workerStatus.resentPosition)
                {
                    auto metadata = takeoverResendPositions.find(*workerStatus.resentPosition);
                    if (metadata == takeoverResendPositions.end())
                    {
                        metadata = takeoverResendPositions.emplace(*workerStatus.resentPosition,
                                                                   PositionObservationMetadata{*workerStatus.resentPosition}).first;
                    }
                    metadata->second.trackFailure();

#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(workerStatus.worker->id) << "Tracking takeover failure by patch switch on " << metadata->second;
                    CherryVis::log(workerStatus.resource->id) << "Tracking takeover failure by patch switch on " << metadata->second;
#endif
                }

                return;
            }

            // Can't observe anything else in the case where we switched patches
            if (switchedPatches) return;

            // Track observations on any positions that would have got us to the patch in the right time frame
            bool passedOptimal = false;
            auto optimalIt = workerStatus.positionHistory.rbegin() + BWAPI::Broodwar->getLatencyFrames() + 10;
            auto resendIt = workerStatus.positionHistory.rend();
            auto secondResendIt = workerStatus.positionHistory.rend();
#if OPTIMALPOSITIONS_DEBUG
            std::ostringstream dbg;
            dbg << "Tracking takeover observation on:";
#endif
            for (auto it = workerStatus.positionHistory.rbegin(); it != workerStatus.positionHistory.rend(); it++)
            {
                // Any resends that got us to the patch in time are observed
                if (!passedOptimal)
                {
                    auto metadata = takeoverResendPositions.find(**it);
                    if (metadata == takeoverResendPositions.end())
                    {
                        metadata = takeoverResendPositions.emplace(**it, PositionObservationMetadata{**it}).first;
                    }
                    metadata->second.trackObservation();
#if OPTIMALPOSITIONS_DEBUG
                    dbg << "\n" << metadata->second;
#endif
                }

                if (workerStatus.resentPosition == *it) resendIt = it;
                if (workerStatus.secondResentPosition == *it) secondResendIt = it;

                if (it == optimalIt) passedOptimal = true;

                if (passedOptimal && (!workerStatus.resentPosition || resendIt != workerStatus.positionHistory.rend()) &&
                    (!workerStatus.secondResentPosition || secondResendIt != workerStatus.positionHistory.rend()))
                {
                    break;
                }
            }

#if OPTIMALPOSITIONS_DEBUG
            CherryVis::log(workerStatus.worker->id) << dbg.str();
            CherryVis::log(workerStatus.resource->id) << dbg.str();
#endif

            // Track success or failure of resend
            if (workerStatus.resentPosition && resendIt != workerStatus.positionHistory.rend())
            {
                auto resendPositionData = takeoverResendPositions.find(*workerStatus.resentPosition);
                if (resendPositionData == takeoverResendPositions.end())
                {
                    resendPositionData = takeoverResendPositions.emplace(*workerStatus.resentPosition,
                                                                         PositionObservationMetadata{*workerStatus.resentPosition}).first;
                }

                auto resendDelta = (int)std::distance(resendIt, optimalIt);
                auto secondResendDelta =
                        (workerStatus.secondResentPosition && secondResendIt != workerStatus.positionHistory.rend())
                        ? (int)std::distance(secondResendIt, optimalIt)
                        : INT_MAX;
                resendPositionData->second.trackNonoptimalResend(**optimalIt,
                                                                 workerStatus.secondResentPosition,
                                                                 resendDelta,
                                                                 secondResendDelta);

#if OPTIMALPOSITIONS_DEBUG
                if (resendDelta >= 0)
                {
                    CherryVis::log(workerStatus.worker->id) << "Tracking success on takeover " << resendPositionData->second;
                    CherryVis::log(workerStatus.resource->id) << "Tracking success on takeover " << resendPositionData->second;
                }
                else
                {
                    CherryVis::log(workerStatus.worker->id) << "Tracking failure on takeover " << resendPositionData->second;
                    CherryVis::log(workerStatus.resource->id) << "Tracking failure on takeover " << resendPositionData->second;
                }
#endif
            }
        }

        void optimizeArrival(const MyWorker &worker,
                             const Resource &resource,
                             WorkerGatherStatus &workerStatus,
                             std::map<PositionAndVelocity, PositionObservationMetadata> &optimalGatherPositions,
                             const std::shared_ptr<PositionAndVelocity> &currentPosition)
        {
            if (workerStatus.secondResentPosition || workerStatus.resentOnSchedule()) return;

            auto handleOrderProcessTimerReset = [&](unsigned int expectedDelay)
            {
                int framesFromCommandToReset = OrderProcessTimer::framesToNextReset() - BWAPI::Broodwar->getLatencyFrames();
                if (framesFromCommandToReset > 0 && framesFromCommandToReset < (12 + expectedDelay))
                {
                    // Send a command to take effect on the reset frame if it is coming soon
                    // Otherwise just let it take its course
                    if (framesFromCommandToReset < 5)
                    {
                        workerStatus.resendCommandOnFrame = currentFrame + framesFromCommandToReset;
#if OPTIMALPOSITIONS_DEBUG
                        CherryVis::log(worker->id) << "Scheduled gather command for approach optimization on frame "
                                                   << workerStatus.resendCommandOnFrame;
                        CherryVis::log(resource->id) << "Scheduled gather command for approach optimization on frame "
                                                     << workerStatus.resendCommandOnFrame;
#endif

                    }
                    return true;
                }

                return false;
            };

            if (!workerStatus.resentPosition)
            {
                auto optimalGatherPositionIt = optimalGatherPositions.find(*currentPosition);
                unsigned int expectedDelay = 0;
                if (optimalGatherPositionIt != optimalGatherPositions.end() &&
                    shouldResendGatherCommand(worker, optimalGatherPositionIt->second, expectedDelay))
                {
                    if (handleOrderProcessTimerReset(expectedDelay))
                    {
                    }
                    else if (worker->gather(resource->getBwapiUnitIfVisible()))
                    {
                        workerStatus.resentPosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                        CherryVis::log(worker->id) << "Resending gather command for approach optimization at position "
                                                   << optimalGatherPositionIt->second;
                        CherryVis::log(resource->id) << "Resending gather command for approach optimization at position "
                                                     << optimalGatherPositionIt->second;
#endif
                    }
                    else
                    {
                        workerStatus.resendCommandOnFrame = (currentFrame + 1);

#if OPTIMALPOSITIONS_DEBUG
                        Log::Get() << "Failed to send gather command for " << worker->id << " @ " << worker->getTilePosition() << ": "
                                   << BWAPI::Broodwar->getLastError();
                        CherryVis::log(worker->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
                        CherryVis::log(resource->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
#endif
                    }
                }

                return;
            }

            auto resendPositionDataIt = optimalGatherPositions.find(*workerStatus.resentPosition);
            if (resendPositionDataIt == optimalGatherPositions.end()) return;

            auto &resendPositionData = resendPositionDataIt->second;

            auto optimalGatherPositionIt = resendPositionData.failurePositionMetadata.find(*currentPosition);
            bool mayGetUnitBusy = false;
            if (optimalGatherPositionIt != resendPositionData.failurePositionMetadata.end() &&
                shouldResendGatherCommand(worker, optimalGatherPositionIt->second, mayGetUnitBusy))
            {
                if (handleOrderProcessTimerReset(0))
                {
                }
                else if (mayGetUnitBusy)
                {
                    // Sending the command now will result in Unit_Busy, so schedule it for the next frame
                    workerStatus.resendCommandOnFrame = (currentFrame + 1);
                    workerStatus.secondResentPosition = currentPosition;
                }
                else if (worker->gather(resource->getBwapiUnitIfVisible()))
                {
                    workerStatus.secondResentPosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(worker->id) << "Resending second gather command for approach optimization";
                    CherryVis::log(resource->id) << "Resending second gather command for approach optimization";
#endif

                }
                else
                {
                    workerStatus.resendCommandOnFrame = (currentFrame + 1);

#if OPTIMALPOSITIONS_DEBUG
                    Log::Get() << "Failed to send second gather command for approach optimization for " << worker->id << " @ "
                               << worker->getTilePosition() << ": " << BWAPI::Broodwar->getLastError();
                    CherryVis::log(worker->id) << "Failed to send second gather command for approach optimization; last error "
                                               << BWAPI::Broodwar->getLastError();
                    CherryVis::log(resource->id) << "Failed to send second gather command for approach optimization; last error "
                                                 << BWAPI::Broodwar->getLastError();
#endif
                }
            }
        }
    }

    void initialize()
    {
        workerGatherStatuses.clear();

        parsePositionObservationMetadataFile(optimalGatherPositionsFilename(), resourceToOptimalGatherPositions);
        parsePositionObservationMetadataFile(tenDistancePositionsFilename(), resourceTo10DistancePositions);
        parsePositionObservationMetadataFile(takeoverResendPositionsFilename(), resourceToTakeoverResendPositions);
    }

    void flushObservations()
    {
#if !SAVED_POSITIONS_ONLY
        // Flush the worker statuses for workers that have started mining
        for (auto it = workerGatherStatuses.begin(); it != workerGatherStatuses.end(); )
        {
            auto &worker = it->first;
            if (!worker->exists())
            {
                it = workerGatherStatuses.erase(it);
                continue;
            }

            if (worker->bwapiUnit->getOrder() != BWAPI::Orders::WaitForMinerals)
            {
                it++;
                continue;
            }

            auto &workerStatus = it->second;

            // Remove positions before the worker reached the patch
            auto positionIt = workerStatus.positionHistory.begin();
            for (; positionIt != workerStatus.positionHistory.end(); positionIt++)
            {
                auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                    (*positionIt)->pos(),
                                                    BWAPI::UnitTypes::Resource_Mineral_Field,
                                                    workerStatus.resource->center);
                if (dist == 0 && worker->lastPosition.getApproxDistance((*positionIt)->pos()) < 2) break;
            }
            workerStatus.positionHistory.erase(positionIt, workerStatus.positionHistory.end());

            // Ensure we have enough history
            if (workerStatus.positionHistory.size() >= (BWAPI::Broodwar->getLatencyFrames() + 11))
            {
                // Handle observations for the optimizing arrival at the patch
                auto optimalPositionIt = workerStatus.positionHistory.rbegin() + BWAPI::Broodwar->getLatencyFrames() + 10;
                handleObservation(workerStatus,
                                  *optimalPositionIt,
                                  workerStatus.resentPosition,
                                  resourceToOptimalGatherPositions[workerStatus.resource]);

                // Tracking of 10-distance positions and resend positions for takeover
                updateTakeoverMetadata(workerStatus,
                                       workerStatus.resource,
                                       resourceTo10DistancePositions[workerStatus.resource],
                                       resourceToTakeoverResendPositions[workerStatus.resource]);
            }

            // We now no longer need to do anything with this worker status
            it = workerGatherStatuses.erase(it);
        }
#endif
    }

    void write()
    {
#if SAVED_POSITIONS_ONLY
        return;
#endif

        writePositionObservationMetadataFile(optimalGatherPositionsFilename(true), resourceToOptimalGatherPositions);
        writePositionObservationMetadataFile(tenDistancePositionsFilename(true), resourceTo10DistancePositions);
        writePositionObservationMetadataFile(takeoverResendPositionsFilename(true), resourceToTakeoverResendPositions);
    }

    // Optimizes the start of mining, returning whether an order was sent to the worker.
    void optimizeStartOfMining(const MyWorker &worker, const Resource &resource)
    {
        auto &optimalGatherPositions = resourceToOptimalGatherPositions[resource];
        auto &tenDistancePositions = resourceTo10DistancePositions[resource];
        auto &takeoverResendPositions = resourceToTakeoverResendPositions[resource];
        auto workerStatusIt = workerGatherStatuses.find(worker);
        if (workerStatusIt == workerGatherStatuses.end())
        {
            workerStatusIt = workerGatherStatuses.emplace(worker, WorkerGatherStatus{worker, resource}).first;
        }
        auto &workerStatus = workerStatusIt->second;

        auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
        if (!resourceBwapiUnit)
        {
            CherryVis::log(worker->id) << "mineral field unit not visible";
            workerStatus.reset();
            return;
        }

        auto currentPosition = std::make_shared<PositionAndVelocity>(worker);

        // Clear worker status if it wasn't processed last frame
        if (workerStatus.lastProcessedFrame != (currentFrame - 1))
        {
            workerStatus.reset();
        }
        workerStatus.lastProcessedFrame = currentFrame;

        // Track the worker's visited positions
        workerStatus.positionHistory.emplace_back(currentPosition);

        // Don't touch the worker if it is transitioning to mine
        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals) return;

        // Our logic ensures mineral locking automatically except in some specific cases:
        // - worker has been released from combat, which can leave it with a gather order to a random patch used for kiting
        // - workers have been avoiding a no-go area and returning to mining as a group, so the timing gets messed up
        // - we don't have enough observed resend positions and get unlucky on the order timer
        if (worker->bwapiUnit->getOrderTarget() && worker->bwapiUnit->getOrderTarget()->getResources()
            && worker->bwapiUnit->getOrderTarget() != resourceBwapiUnit
            && worker->lastCommandFrame < (currentFrame - BWAPI::Broodwar->getLatencyFrames()))
        {
            // We want to update our metadata if a 10-distance position or takeover resend position didn't work
            updateTakeoverMetadata(workerStatus, resource, tenDistancePositions, takeoverResendPositions, true);

            CherryVis::log(worker->id) << "targeting different patch; resending order";
            Log::Get() << "patch @ " << resource->tile << "; worker " << worker->id << " @ " << worker->getTilePosition() << " switched patch";

            worker->gather(resourceBwapiUnit);
            workerStatus.positionHistory.clear();
            workerStatus.takeoverMode = 2;
            workerStatus.lastProcessedFrame = currentFrame;
            return;
        }

        // Resend the gather command if it has been scheduled for this frame
        if (workerStatus.resendCommandOnFrame == currentFrame)
        {
            worker->gather(resourceBwapiUnit);

#if OPTIMALPOSITIONS_DEBUG
            CherryVis::log(worker->id) << "Resending gather command on schedule";
            CherryVis::log(resource->id) << "Resending gather command on schedule";
#endif
            return;
        }

        // Handle case where another worker is assigned to the patch
        auto otherWorker = Workers::getOtherWorkerMining(resource, worker);
        if (otherWorker && otherWorker->exists() && (currentFrame - otherWorker->lastStartedMining) < 100)
        {
            // Keep track of whether the worker has passed a 10-distance position
            if (!workerStatus.passed10DistancePosition && tenDistancePositions.contains(*currentPosition))
            {
                workerStatus.passed10DistancePosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(worker->id) << "Will reach 10-distance position in LF+1 from here";
                CherryVis::log(resource->id) << "Will reach 10-distance position in LF+1 from here";
#endif
            }

            // Compute the optimal frame to take over from the other worker

            // We need to add an extra frame if the worker taking over might have its orders processed first
            int addedFrame = 1;
            if (otherWorker->orderProcessIndex > worker->orderProcessIndex)
            {
                addedFrame = 0;
            }

            // Without order timer resets, we can compute the exact takeover frame
            int takeOverFrame = otherWorker->lastStartedMining + 81 + addedFrame;

            // Compute the frame of the order timer reset prior to the take over frame
            int previousOrderTimerReset = OrderProcessTimer::previousResetFrame(takeOverFrame);
            if (previousOrderTimerReset == takeOverFrame) previousOrderTimerReset -= 150;

            if (previousOrderTimerReset == (otherWorker->lastStartedMining - 1))
            {
                Log::Get() << "patch @ " << resource->tile << "; worker " << worker->id << " @ " << worker->getTilePosition() << " -1";
            }

            // If the order timer reset during mining, adjust our take over frame
            // We always assume the worst-case scenario (needing to wait a full cycle after the mining timer expires)
            // Because the order timer is at 6 when mining ends without a reset, we only have to wait two extra frames
            if (previousOrderTimerReset >= otherWorker->lastStartedMining)
            {
                takeOverFrame = std::max(otherWorker->lastStartedMining + 83, previousOrderTimerReset + 8) + addedFrame;
            }

            // Now compute when we need to issue mining commands
            // Besides issuing a mining command for the takeover frame, we also want to issue a command if the order timer resets
            int commandFrameForTakeOver = takeOverFrame - 11 - BWAPI::Broodwar->getLatencyFrames();
            int commandFrameForReset = previousOrderTimerReset - BWAPI::Broodwar->getLatencyFrames();

            // If the takeover frame comes first, delay sending the order so it takes effect when the order timer resets instead
            // This is to avoid situations where the second worker's command takes effect too soon, causing it to switch to a different patch
            if (commandFrameForReset > commandFrameForTakeOver)
            {
                commandFrameForTakeOver = commandFrameForReset;
            }

            // Compute the number of frames until the next command we have to send
            // We ignore order process timer resets if we are far enough away from the patch that it will not affect us
            auto distToPatch = resource->getDistance(worker);
            int framesToNextCommand;
            if (currentFrame <= commandFrameForReset && (commandFrameForTakeOver - commandFrameForReset) > 3
                && (workerStatus.passed10DistancePosition || distToPatch == 0))
            {
                framesToNextCommand = std::min(commandFrameForReset, commandFrameForTakeOver) - currentFrame;
            }
            else
            {
                framesToNextCommand = commandFrameForTakeOver - currentFrame;
            }

#if TAKEOVER_DEBUG
            CherryVis::log(worker->id)
                    << "Timing for takeover from " << otherWorker->id << ": "
                    << "otherStarted=" << otherWorker->lastStartedMining << "; "
                    << "takeOverFrame=" << takeOverFrame << "; "
                    << "previousOrderTimerReset=" << previousOrderTimerReset << "; "
                    << "commandFrameForTakeOver=" << commandFrameForTakeOver << "; "
                    << "commandFrameForReset=" << commandFrameForReset << "; "
                    << "framesToNextCommand=" << framesToNextCommand << "; "
                    << "addedFrame=" << addedFrame << "; "
                    << "distToPatch=" << distToPatch << "; "
                    << "passed10Distance=" << (workerStatus.passed10DistancePosition != nullptr) << "; "
                    << "resentPosition=" << (workerStatus.resentPosition != nullptr) << "; "
                    << "takeoverMode=" << workerStatus.takeoverMode;
#endif

            // Logic for when the next command is in the future
            if (framesToNextCommand > 0)
            {
                // Reset some state in case we needed to resend because of the order timer
                workerStatus.resentPosition = nullptr;
                workerStatus.secondResentPosition = nullptr;
                workerStatus.takeoverMode = 0;

                // Once the worker is close to the patch, resend commands every 2 frames to avoid the worker switching patches
                if (framesToNextCommand % 2 == 0 && (workerStatus.passed10DistancePosition || distToPatch == 0))
                {
                    worker->gather(resourceBwapiUnit);
                }

                return;
            }

            switch (workerStatus.takeoverMode)
            {
                case 0:
                {
                    // Try to use normal approach optimization, which applies if the worker is not able to reach the patch by the takeover frame
                    if (distToPatch > 0)
                    {
                        optimizeArrival(worker, resource, workerStatus, optimalGatherPositions, currentPosition);
                        if (workerStatus.resentPosition || workerStatus.resentOnSchedule()) return;
                    }

                    // intentional fall-through
                }
                case 1:
                {
                    if (distToPatch == 0 && !workerStatus.resentPosition && !workerStatus.resentOnSchedule())
                    {
                        workerStatus.takeoverMode = 2;
                        worker->gather(resource->getBwapiUnitIfVisible());

#if OPTIMALPOSITIONS_DEBUG
                        CherryVis::log(worker->id) << "Resending gather command for takeover optimization, as have arrived at patch";
                        CherryVis::log(resource->id) << "Resending gather command for takeover optimization, as have arrived at patch";
#endif
                        break;
                    }

                    auto takeoverPositionValid = [&](const PositionObservationMetadata &takeoverPositionData)
                    {
#if OPTIMALPOSITIONS_DEBUG
                        if (takeoverPositionData.failures > 0)
                        {
                            CherryVis::log(worker->id) << "Rejecting for takeover optimization: " << takeoverPositionData;
                            CherryVis::log(resource->id) << "Rejecting for takeover optimization: " << takeoverPositionData;
                        }
#endif

                        // TODO: Revisit the logic after performing some analysis
                        return takeoverPositionData.failures == 0;
                    };

                    // Try to use takeover optimization
                    // This differs from approach optimization as it just tries to ensure the worker will reach the patch on time, not what the order
                    // process timer is
                    if (!workerStatus.resentPosition && !workerStatus.resentOnSchedule())
                    {
                        // Use the first valid position we come to
                        auto takeoverPositionIt = takeoverResendPositions.find(*currentPosition);
                        if (takeoverPositionIt != takeoverResendPositions.end() && takeoverPositionValid(takeoverPositionIt->second))
                        {
                            workerStatus.takeoverMode = 1;

                            if (worker->gather(resource->getBwapiUnitIfVisible()))
                            {
                                workerStatus.resentPosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                                CherryVis::log(worker->id) << "Resending gather command for takeover optimization at position " << takeoverPositionIt->second;
                                CherryVis::log(resource->id) << "Resending gather command for takeover optimization at position "
                                                             << takeoverPositionIt->second;
#endif
                            }
                            else
                            {
                                workerStatus.resendCommandOnFrame = (currentFrame + 1);

#if OPTIMALPOSITIONS_DEBUG
                                Log::Get() << "Failed to send gather command for takeover optimization for " << worker->id << " @ "
                                           << worker->getTilePosition() << ": " << BWAPI::Broodwar->getLastError();
                                CherryVis::log(worker->id) << "Failed to send gather command for takeover optimization; last error "
                                                           << BWAPI::Broodwar->getLastError();
                                CherryVis::log(resource->id) << "Failed to send gather command for takeover optimization; last error "
                                                             << BWAPI::Broodwar->getLastError();
#endif
                            }
                        }
                        else if (workerStatus.passed10DistancePosition)
                        {
                            // As we've passed the 10-distance position, we had better send a command here
                            // We probably just lack the data needed to know which position to send from
                            workerStatus.takeoverMode = 1;
                            worker->gather(resource->getBwapiUnitIfVisible());
                            workerStatus.resentPosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                            CherryVis::log(worker->id) << "Resending as we have reached 10 distance and haven't observed a good position yet";
                            CherryVis::log(resource->id) << "Resending as we have reached 10 distance and haven't observed a good position yet";
#endif
                        }
                    }
                    break;
                }
                case 2:
                {
                    // Worker is at patch and a command has been sent, so no further orders needed
                    return;
                }
            }

            return;
        }

        // Single worker approach optimization
        optimizeArrival(worker, resource, workerStatus, optimalGatherPositions, currentPosition);
    }

    // Optimizes returning a resource, returning whether an order was sent to the worker.
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
//        if (resource && depot->getDistance(worker) == 0 && worker->orderProcessTimer == (BWAPI::Broodwar->getLatencyFrames() - 2)
//            && OrderProcessTimer::framesToNextReset() > BWAPI::Broodwar->getLatencyFrames())
//        {
//            auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
//            if (resourceBwapiUnit)
//            {
//                worker->gather(resourceBwapiUnit);
//            }
//        }

        // TODO
    }
}