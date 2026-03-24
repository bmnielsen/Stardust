#include "DataTransformer.h"

#include "MiningOptimizationV2/DataModel/MapData.h"
#include "MiningOptimizationV2/DataModel/Serialization.h"

namespace MiningOptimizationTraining::DataTransformer
{
    namespace
    {
        // Gets the total occurrences from a next positions vector or arrival observations map
        uint32_t getTotalOccurrences(const auto &observations)
        {
            uint32_t totalOccurrences = 0;
            for (const auto &[_, occurrences] : observations) totalOccurrences += occurrences;
            return totalOccurrences;
        }

        uint8_t computeOccurrenceRate(const auto &observations, std::unique_ptr<uint32_t> &totalOccurrences, uint32_t occurrences)
        {
            if (observations.size() == 1) return OCCURRENCE_SCALE;
            if (!totalOccurrences) totalOccurrences = std::make_unique<uint32_t>(getTotalOccurrences(observations));
            return (uint8_t)(std::round((double)OCCURRENCE_SCALE * ((double)occurrences / (double)(*totalOccurrences))));
        }

        // Ensures that the total occurrence rate is 255
        void postProcessObservationsVector(auto &observations)
        {
            if (observations.empty()) return;
            if (observations.size() == 1)
            {
                // This is needed since there might be rounding issues for cases where multiple observations were collapsed into one
                observations.begin()->second = OCCURRENCE_SCALE;
                return;
            }

            // Get the total and check if it is 255, also tracking what the max occurrence count is and removing any with a 0 rate
            unsigned int totalOccurrences = 0;
            for (auto it = observations.begin(); it != observations.end(); )
            {
                if (it->second == 0)
                {
                    it = observations.erase(it);
                    continue;
                }

                totalOccurrences += it->second;
                it++;
            }
            while (totalOccurrences != OCCURRENCE_SCALE)
            {
                // Get the max occurrences
                uint8_t maxOccurrences = 0;
                for (auto &[_, occurrences] : observations) maxOccurrences = std::max(maxOccurrences, occurrences);

                // Increment or decrement the observation with the max occurrence count
                for (auto &[_, occurrences] : observations)
                {
                    if (occurrences != maxOccurrences) continue;
                    if (totalOccurrences > OCCURRENCE_SCALE)
                    {
                        occurrences--;
                        totalOccurrences--;
                    }
                    else
                    {
                        occurrences++;
                        totalOccurrences++;
                    }
                    break;
                }
            }

            std::sort(observations.begin(), observations.end(), [](const auto &a, const auto &b)
            {
                return a.second > b.second;
            });
        }

        template <typename ObservationType>
        void gatherPositionDeltas(const PositionAndVelocity &pos, // NOLINT(*-no-recursion)
                                  const std::vector<std::pair<PathNode<ObservationType>, uint32_t>> &nextNodes,
                                  std::vector<std::pair<int8_t, int8_t>> &positionDeltas,
                                  std::map<std::pair<int8_t, int8_t>, uint8_t> &positionDeltaToIndex)
        {
            for (const auto &[nextNode, _] : nextNodes)
            {
                auto delta = std::make_pair((int8_t)(nextNode.pos.x - pos.x), (int8_t)(nextNode.pos.y - pos.y));
                if (!positionDeltaToIndex.contains(delta))
                {
                    positionDeltas.push_back(delta);
                    positionDeltaToIndex[delta] = (uint8_t)(positionDeltas.size() - 1);
                    EXPECT_GE(128, positionDeltas.size()) << "Positions delta exceeded limit";
                }

                gatherPositionDeltas(nextNode.pos, nextNode.nextPositions, positionDeltas, positionDeltaToIndex);
                gatherPositionDeltas(nextNode.pos, nextNode.nextPositionsAfterResend, positionDeltas, positionDeltaToIndex);
            }
        }

        template <typename ObservationType>
        void gatherPositionDeltas(const std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, Path<ObservationType>>> &pathData,
                                  std::vector<std::pair<int8_t, int8_t>> &positionDeltas,
                                  std::map<std::pair<int8_t, int8_t>, uint8_t> &positionDeltaToIndex)
        {
            for (const auto &[_, rootNodes] : pathData)
            {
                for (const auto &[_, rootNode] : rootNodes)
                {
                    for (auto &[_, nextPositions] : rootNode.nextPositions)
                    {
                        gatherPositionDeltas(rootNode.pos, nextPositions, positionDeltas, positionDeltaToIndex);
                    }
                }
            }
        }

        void gatherTenDistanceAndResendAlwaysArrives( // NOLINT(*-no-recursion)
                const std::vector<std::pair<PathNode<GatherArrivalData>, uint32_t>> &nextNodes,
                std::map<std::pair<int8_t, int8_t>, unsigned long> &tenDistanceAndResendAlwaysArrivesToOccurrences)
        {
            for (const auto &[nextNode, _] : nextNodes)
            {
                if (nextNode.type == NodeType::FinalResendNode)
                {
                    for (const auto &[arrivalData, _] : nextNode.arrivalDataAfterResend)
                    {
                        if (arrivalData.tenDistanceDelta == UINT8_MAX || arrivalData.resendAlwaysArrivesDelta == UINT8_MAX) continue;
                        tenDistanceAndResendAlwaysArrivesToOccurrences[{arrivalData.tenDistanceDelta, arrivalData.resendAlwaysArrivesDelta}]++;
                    }
                }

                gatherTenDistanceAndResendAlwaysArrives(nextNode.nextPositions, tenDistanceAndResendAlwaysArrivesToOccurrences);
                gatherTenDistanceAndResendAlwaysArrives(nextNode.nextPositionsAfterResend, tenDistanceAndResendAlwaysArrivesToOccurrences);
            }
        }

        void gatherTenDistanceAndResendAlwaysArrives(
                const std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, Path<GatherArrivalData>>> &pathData,
                std::map<std::pair<int8_t, int8_t>, unsigned long> &tenDistanceAndResendAlwaysArrivesToOccurrences)
        {
            for (const auto &[_, rootNodes] : pathData)
            {
                for (const auto &[_, rootNode] : rootNodes)
                {
                    for (auto &[_, nextPositions] : rootNode.nextPositions)
                    {
                        gatherTenDistanceAndResendAlwaysArrives(nextPositions, tenDistanceAndResendAlwaysArrivesToOccurrences);
                    }
                }
            }
        }

        template <typename ObservationType>
        void gatherAveragePathArrivalDelays(
                const std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, Path<ObservationType>>> &pathData,
                std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, unsigned int>> &averageArrivalDelays,
                unsigned int &minAverageArrivalDelay)
        {
            for (const auto &[tile, rootNodes] : pathData)
            {
                auto &patchAverageArrivalDelays = averageArrivalDelays[tile];
                for (const auto &[pos, rootNode] : rootNodes)
                {
                    unsigned long delayAccumulator = 0;
                    unsigned long occurrenceOccumulator = 0;
                    for (const auto &[delay, occurrences] : rootNode.bestArrivalDelaysAndOccurrences)
                    {
                        delayAccumulator += delay * occurrences;
                        occurrenceOccumulator += occurrences;
                    }
                    if (occurrenceOccumulator == 0) continue;

                    auto result = ((unsigned int)std::round((double)delayAccumulator / (double)occurrenceOccumulator));
                    patchAverageArrivalDelays[pos] = result;
                    minAverageArrivalDelay = std::min(minAverageArrivalDelay, result);
                }
            }
        }

        void convertAverageArrivalDelays(const std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, unsigned int>> &raw,
                                         std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, uint8_t>> &out,
                                         unsigned int minAverageArrivalDelay)
        {
            for (const auto &[tile, rootNodes] : raw)
            {
                auto &patchOut = out[tile];
                for (const auto &[pos, averageArrivalDelay] : rootNodes)
                {
                    EXPECT_GE(averageArrivalDelay, minAverageArrivalDelay) << "Average arrival delay is smaller than minimum average arrival delay";
                    auto converted = averageArrivalDelay - minAverageArrivalDelay;
                    EXPECT_LE(converted, 63) << "Converted average arrival delay is too big for 6-bit integer";
                    patchOut[pos] = (uint8_t)converted;
                }
            }
        }

        MiningOptimization::PositionAndVelocity convert(const PositionAndVelocity &pos)
        {
            return {pos.x, pos.y, pos.heading, (int16_t)pos.velocityX, (int16_t)pos.velocityY, false};
        }

        MiningOptimization::PositionDeltaAndVelocity delta(const PositionAndVelocity &pos,
                                                             const PositionAndVelocity &next,
                                                             const std::map<std::pair<int8_t, int8_t>, uint8_t> &positionDeltaToIndex)
        {
            auto diff = std::make_pair((int8_t)(next.x - pos.x), (int8_t)(next.y - pos.y));
            uint8_t packed = (positionDeltaToIndex.at(diff)) << 1;
            return {packed, next.heading, (int16_t)next.velocityX, (int16_t)next.velocityY};
        }

        template <typename TrainingObservationType, typename OutputObservationType>
        OutputObservationType convert(const TrainingObservationType &arrivalData,
                                      const std::unordered_map<PositionAndVelocity, uint8_t> &nextPathArrivalDelays)
        {
            // Pack the arrival delay with whatever is already packed in the arrival data
            // For gather this is the collision and facing patch, for return this is the exit speed
            uint8_t packed = (std::min(arrivalData.arrivalDelay(), 63U) << 2) + (arrivalData.packed & 0b00000011);

#if USE_NEXT_PATH_LENGTHS
            // Look up the next path arrival delay, defaulting to the max value if it isn't found
            auto it = nextPathArrivalDelays.find(arrivalData.nextPathStartPosition);
            uint8_t nextPathArrivalDelay = (it == nextPathArrivalDelays.end()) ? 255 : (std::min(it->second, (uint8_t)255U));

            return {packed, nextPathArrivalDelay};
#else
            return {packed};
#endif
        }

        template <typename TrainingObservationType, typename OutputObservationType>
        OutputObservationType convert(const TrainingObservationType &arrivalData,
                                      const std::map<std::pair<int8_t, int8_t>, uint8_t> &tenDistanceAndResendAlwaysArrivesToIndex,
                                      const std::unordered_map<PositionAndVelocity, uint8_t> &nextPathArrivalDelays)
        {
            return convert<TrainingObservationType, OutputObservationType>(arrivalData, nextPathArrivalDelays);
        }

        template <>
        MiningOptimization::GatherArrivalData convert(const GatherArrivalData &arrivalData,
                                                      const std::map<std::pair<int8_t, int8_t>, uint8_t> &tenDistanceAndResendAlwaysArrivesToIndex,
                                                      const std::unordered_map<PositionAndVelocity, uint8_t> &nextPathArrivalDelays)
        {
            auto result = convert<GatherArrivalData, MiningOptimization::GatherArrivalData>(arrivalData, nextPathArrivalDelays);

            auto it = tenDistanceAndResendAlwaysArrivesToIndex.find(
                    {arrivalData.tenDistanceDelta, arrivalData.resendAlwaysArrivesDelta});
            if (it != tenDistanceAndResendAlwaysArrivesToIndex.end())
            {
                result.tenDistanceAndResendAlwaysArrivesIndex = it->second;
            }

            return result;
        }

        template <>
        MiningOptimization::ReturnArrivalData convert(const ReturnArrivalData &arrivalData,
                                                      const std::map<std::pair<int8_t, int8_t>, uint8_t> &tenDistanceAndResendAlwaysArrivesToIndex,
                                                      const std::unordered_map<PositionAndVelocity, uint8_t> &nextPathArrivalDelays)
        {
            auto result = convert<ReturnArrivalData, MiningOptimization::ReturnArrivalData>(arrivalData, nextPathArrivalDelays);
            result.collision = arrivalData.collision();
            return result;
        }

        template <typename TrainingObservationType, typename OutputObservationType>
        std::vector<std::pair<MiningOptimization::PathNode<OutputObservationType>, uint8_t>> convert( // NOLINT(*-no-recursion)
                const PositionAndVelocity &pos,
                const std::vector<std::pair<PathNode<TrainingObservationType>, uint32_t>> &nextNodes,
                const std::map<std::pair<int8_t, int8_t>, uint8_t> &positionDeltaToIndex,
                const std::map<std::pair<int8_t, int8_t>, uint8_t> &tenDistanceAndResendAlwaysArrivesToIndex,
                const std::unordered_map<PositionAndVelocity, uint8_t> &nextPathArrivalDelays)
        {
            if (nextNodes.empty()) return {};

            auto convertArrivalData =
                    [&](const std::map<TrainingObservationType, uint32_t> &observations) -> std::vector<std::pair<OutputObservationType, uint8_t>>
            {
                if (observations.empty()) return {};

                std::unique_ptr<uint32_t> totalArrivalOccurrences;
                std::vector<std::pair<OutputObservationType, uint8_t>> result;
                for (const auto &[arrivalData, occurrences] : observations)
                {
                    // Arrival data with different next path start positions may have the same next path length, causing them to become
                    // equal after conversion.
                    // We therefore take this into consideration and check if there is a preexisting match
                    auto converted = convert<TrainingObservationType, OutputObservationType>(arrivalData,
                                                                                             tenDistanceAndResendAlwaysArrivesToIndex,
                                                                                             nextPathArrivalDelays);
                    auto it = std::find_if(result.begin(), result.end(), [&converted](const std::pair<OutputObservationType, uint8_t> &item)
                    {
                        return item.first == converted;
                    });

                    // No match; just emplace the new value
                    if (it == result.end())
                    {
                        result.emplace_back(std::move(converted), computeOccurrenceRate(observations, totalArrivalOccurrences, occurrences));
                        continue;
                    }

                    // There is a match, so get the occurrence rate and either add it or set to max if there would be an overflow
                    auto occurrenceRate = computeOccurrenceRate(observations, totalArrivalOccurrences, occurrences);
                    if ((unsigned int)it->second + (unsigned int)occurrenceRate > OCCURRENCE_SCALE)
                    {
                        it->second = OCCURRENCE_SCALE;
                    }
                    else
                    {
                        it->second += occurrenceRate;
                    }
                }

                postProcessObservationsVector(result);

                // To store the resend viability bool, we borrow a bit where the first arrivalDataAfterResend item's occurrence rate would go
                // Validate that this will never collide with an actual value
                if (!result.empty())
                {
                    EXPECT_NE((uint8_t)1, result.begin()->second);
                }

                return result;
            };

            auto convertNode =
                    [&](const PathNode<TrainingObservationType> &node) // NOLINT(*-no-recursion)
                    -> MiningOptimization::PathNode<OutputObservationType>
            {
                // Perform validations based on the node type to ensure we have the expected data
                switch (node.type)
                {
                    case NodeType::Uninitialized:
                        EXPECT_FALSE(true) << "There should not be uninitialized nodes in the training data";
                        break;
                    case NodeType::AfterExplorationWindow:
                        EXPECT_FALSE(node.arrivalData.empty());
                        EXPECT_TRUE(node.arrivalDataAfterResend.empty());
                        EXPECT_TRUE(node.nextPositionsAfterResend.empty());
                        break;
                    case NodeType::StableNode:
                    case NodeType::ResendUnavailable:
                        EXPECT_FALSE(node.arrivalData.empty());
                        EXPECT_TRUE(node.arrivalDataAfterResend.empty());
                        EXPECT_FALSE(node.nextPositions.empty());
                        EXPECT_TRUE(node.nextPositionsAfterResend.empty());
                        break;
                    case NodeType::PoorResendNode:
                    case NodeType::FinalResendNode:
                        EXPECT_FALSE(node.arrivalData.empty());
                        EXPECT_FALSE(node.arrivalDataAfterResend.empty());
                        EXPECT_FALSE(node.nextPositions.empty());
                        EXPECT_TRUE(node.nextPositionsAfterResend.empty());
                        break;
                    case NodeType::NonfinalResendNode:
                        EXPECT_FALSE(node.arrivalData.empty());
                        EXPECT_FALSE(node.arrivalDataAfterResend.empty());
                        EXPECT_FALSE(node.nextPositions.empty());
                        EXPECT_FALSE(node.nextPositionsAfterResend.empty());
                        break;
                    case NodeType::Test:
                        // No assertions needed, they are handled in the test
                        break;
                }

                // The arrival data is only needed when this is the final node
                // We convert all of them here for simplicity, and they are then excluded for other nodes in the serialization
                // To ensure data consistency we do some validations here
                if (node.nextPositions.empty() && node.type != NodeType::Test)
                {
                    EXPECT_FALSE(node.arrivalData.empty());
                    for (const auto &[arrivalData, _] : node.arrivalData)
                    {
                        EXPECT_EQ(1, arrivalData.arrivalDelay());
                    }
                }

                return {
                        delta(pos, node.pos, positionDeltaToIndex),
                        convertArrivalData(node.arrivalData),
                        convertArrivalData(node.arrivalDataAfterResend),
                        convert<TrainingObservationType, OutputObservationType>(
                                node.pos,
                                node.nextPositions,
                                positionDeltaToIndex,
                                tenDistanceAndResendAlwaysArrivesToIndex,
                                nextPathArrivalDelays),
                        convert<TrainingObservationType, OutputObservationType>(
                                node.pos,
                                node.nextPositionsAfterResend,
                                positionDeltaToIndex,
                                tenDistanceAndResendAlwaysArrivesToIndex,
                                nextPathArrivalDelays),
                        node.type == NodeType::StableNode};
            };

            std::unique_ptr<uint32_t> totalNodeOccurrences;
            std::vector<std::pair<MiningOptimization::PathNode<OutputObservationType>, uint8_t>> result;
            result.reserve(nextNodes.size());
            for (const auto &[node, occurrences] : nextNodes)
            {
                result.emplace_back(convertNode(node), computeOccurrenceRate(nextNodes, totalNodeOccurrences, occurrences));
            }

            postProcessObservationsVector(result);

            // Flag the positions that need to include the heading and velocity to differentiate between nodes
            for (size_t i = 0; i < result.size(); i++)
            {
                for (size_t j = i + 1; j < result.size(); j++)
                {
                    if (result[i].first.pos.packed != result[j].first.pos.packed) continue;
                    result[i].first.pos.packed += 1;
                    result[j].first.pos.packed += 1;
                }
            }

            return result;
        }

        template <typename TrainingObservationType, typename OutputObservationType>
        std::map<MiningOptimization::CannonPlacement, MiningOptimization::Path<OutputObservationType>> convert(
                const Path<TrainingObservationType> &rootNode,
                const std::map<std::pair<int8_t, int8_t>, uint8_t> &positionDeltaToIndex,
                const std::map<std::pair<int8_t, int8_t>, uint8_t> &tenDistanceAndResendAlwaysArrivesToIndex,
                const std::unordered_map<PositionAndVelocity, uint8_t> &nextPathArrivalDelays)
        {
            std::map<MiningOptimization::CannonPlacement, MiningOptimization::Path<OutputObservationType>> result;

            for (auto &[cannonPlacement, nextPositions] : rootNode.nextPositions)
            {
                result[cannonPlacement] = {
                        convert(rootNode.pos),
                        convert<TrainingObservationType, OutputObservationType>(rootNode.pos,
                                                                                nextPositions,
                                                                                positionDeltaToIndex,
                                                                                tenDistanceAndResendAlwaysArrivesToIndex,
                                                                                nextPathArrivalDelays)};
            }

            return result;
        }

        template <typename TrainingObservationType, typename OutputObservationType>
        void transform(
                const std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, Path<TrainingObservationType>>> &pathData,
                std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, uint8_t>> &nextPathArrivalDelays,
                std::unordered_map<TilePosition, std::unordered_map<MiningOptimization::PositionAndVelocity,
                                                                    MiningOptimization::SerializedPath<OutputObservationType>>> &outputData,
                const std::map<std::pair<int8_t, int8_t>, uint8_t> &positionDeltaToIndex,
                const std::map<std::pair<int8_t, int8_t>, uint8_t> &tenDistanceAndResendAlwaysArrivesToIndex)
        {
            for (const auto &[tile, rootNodes] : pathData)
            {
                // Compute the arrival delays for the next paths
                auto &patchNextPathArrivalDelays = nextPathArrivalDelays[tile];

                // Convert the root nodes
                auto &outputRootNodes = outputData[tile];
                for (const auto &[pos, rootNode] : rootNodes)
                {
                    outputRootNodes[convert(pos)] = MiningOptimization::SerializedPath<OutputObservationType>::create(
                            convert<TrainingObservationType, OutputObservationType>(
                                    rootNode, positionDeltaToIndex, tenDistanceAndResendAlwaysArrivesToIndex, patchNextPathArrivalDelays));
                }
            }
        }
    }

    void transform(const MapData &trainingData)
    {
        MiningOptimization::MapData outputData;
        outputData.mapHash = trainingData.mapHash;

        // Start by finding all of the needed position deltas
        std::cout << "Building position delta map..." << std::endl;
        std::map<std::pair<int8_t, int8_t>, uint8_t> positionDeltaToIndex;
        positionDeltaToIndex[{0, 0}] = 0;
        outputData.positionDeltas.emplace_back(0, 0);
        gatherPositionDeltas(trainingData.resourceToGatherPaths, outputData.positionDeltas, positionDeltaToIndex);
        gatherPositionDeltas(trainingData.resourceToReturnPaths, outputData.positionDeltas, positionDeltaToIndex);
        std::cout << "...found " << outputData.positionDeltas.size() << " position deltas" << std::endl;

        // Next find the ten distance and resend always arrives deltas
        // This only applies to gather and only to final resend nodes
        // There will most likely be more than 256 combinations, so we sort by occurrence count and set a default value for those we can't represent
        std::cout << "Building ten distance and resend always arrives map..." << std::endl;

        // Start by gathering all of the combinations with their occurrence counts
        std::map<std::pair<int8_t, int8_t>, unsigned long> tenDistanceAndResendAlwaysArrivesToOccurrences;
        gatherTenDistanceAndResendAlwaysArrives(trainingData.resourceToGatherPaths, tenDistanceAndResendAlwaysArrivesToOccurrences);

        // Now sort them by occurrence count descending
        std::vector<std::pair<std::pair<uint8_t, uint8_t>, unsigned long>> sortedTenDistanceAndResendAlwaysArrivesToOccurrences;
        sortedTenDistanceAndResendAlwaysArrivesToOccurrences.reserve(tenDistanceAndResendAlwaysArrivesToOccurrences.size());
        for (const auto &item : tenDistanceAndResendAlwaysArrivesToOccurrences)
        {
            sortedTenDistanceAndResendAlwaysArrivesToOccurrences.emplace_back(item);
        }
        std::sort(sortedTenDistanceAndResendAlwaysArrivesToOccurrences.begin(),
                  sortedTenDistanceAndResendAlwaysArrivesToOccurrences.end(),
                  [](const std::pair<std::pair<uint8_t, uint8_t>, unsigned long> &first,
                     const std::pair<std::pair<uint8_t, uint8_t>, unsigned long> &second)
                  {
                      return second.second < first.second;
                  });

        // Now fill in the output data vector and the index lookup map
        outputData.tenDistanceAndResendAlwaysArrives.reserve(std::min(255UL, sortedTenDistanceAndResendAlwaysArrivesToOccurrences.size()));
        std::map<std::pair<int8_t, int8_t>, uint8_t> tenDistanceAndResendAlwaysArrivesToIndex;
        unsigned long handledOccurrences = 0;
        unsigned long unhandledOccurrences = 0;
        for (int i = 0; i < sortedTenDistanceAndResendAlwaysArrivesToOccurrences.size(); i++)
        {
            tenDistanceAndResendAlwaysArrivesToIndex[sortedTenDistanceAndResendAlwaysArrivesToOccurrences[i].first] = std::min(i, 255);
            if (i < 255)
            {
                outputData.tenDistanceAndResendAlwaysArrives.emplace_back(sortedTenDistanceAndResendAlwaysArrivesToOccurrences[i].first);
                handledOccurrences += sortedTenDistanceAndResendAlwaysArrivesToOccurrences[i].second;
            }
            else
            {
                unhandledOccurrences += sortedTenDistanceAndResendAlwaysArrivesToOccurrences[i].second;
            }
        }
        std::cout << std::fixed << std::setprecision(2)
                  << "...found " << sortedTenDistanceAndResendAlwaysArrivesToOccurrences.size() << " combinations"
                  << "; " << handledOccurrences << " occurrences covered"
                  << "; " << unhandledOccurrences
                  << " (" << ((double)unhandledOccurrences * 100.0 / (double)(handledOccurrences + unhandledOccurrences)) << "%)"
                  << " occurrences not covered"
                  << std::endl;

        // Next, compute all of the average arrival delays from each path
        // We do this in two steps - first we get all the values with the minimum, then we prepare for serialization by subtracting the minimum
        std::cout << "Gathering average arrival delays..." << std::endl;

        std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, unsigned int>> rawGatherAverageArrivalDelays;
        std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, unsigned int>> rawReturnAverageArrivalDelays;
        outputData.minimumNextPathLength = UINT_MAX;
        gatherAveragePathArrivalDelays(trainingData.resourceToGatherPaths, rawGatherAverageArrivalDelays, outputData.minimumNextPathLength);
        gatherAveragePathArrivalDelays(trainingData.resourceToReturnPaths, rawReturnAverageArrivalDelays, outputData.minimumNextPathLength);

        std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, uint8_t>> gatherAverageArrivalDelays;
        std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, uint8_t>> returnAverageArrivalDelays;
        convertAverageArrivalDelays(rawGatherAverageArrivalDelays, gatherAverageArrivalDelays, outputData.minimumNextPathLength);
        convertAverageArrivalDelays(rawReturnAverageArrivalDelays, returnAverageArrivalDelays, outputData.minimumNextPathLength);

        std::cout << "...minimum arrival delay is " << outputData.minimumNextPathLength << std::endl;

        // Now transform the data
        std::cout << "Transforming gather data..." << std::endl;
        transform(trainingData.resourceToGatherPaths,
                  returnAverageArrivalDelays,
                  outputData.resourceToSerializedGatherPaths,
                  positionDeltaToIndex,
                  tenDistanceAndResendAlwaysArrivesToIndex);
        std::cout << "...done!" << std::endl;

        std::cout << "Transforming return data..." << std::endl;
        transform(trainingData.resourceToReturnPaths,
                  gatherAverageArrivalDelays,
                  outputData.resourceToSerializedReturnPaths,
                  positionDeltaToIndex,
                  tenDistanceAndResendAlwaysArrivesToIndex);
        std::cout << "...done!" << std::endl;

        // Finally serialize everything
        std::cout << "Serializing map data..." << std::endl;
        MiningOptimization::Serialization::writeMapData(outputData);
        std::cout << "...done!" << std::endl;
    }
}
