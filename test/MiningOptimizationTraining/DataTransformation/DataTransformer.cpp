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
            if (observations.size() == 1) return 255;
            if (!totalOccurrences) totalOccurrences = std::make_unique<uint32_t>(getTotalOccurrences(observations));
            return (uint8_t)(std::round(255.0 * ((double)occurrences / (double)(*totalOccurrences))));
        }

        // Ensures that the total occurrence rate is 255
        void ensureOccurrenceRateTotal(auto &observations)
        {
            if (observations.size() < 2) return;

            // Get the total and check if it is 255, also tracking what the max occurrence count is and removing any with a 0 rate
            unsigned int totalOccurrences = 0;
            uint8_t maxOccurrences = 0;
            for (auto it = observations.begin(); it != observations.end(); )
            {
                if (it->second == 0)
                {
                    it = observations.erase(it);
                    continue;
                }

                totalOccurrences += it->second;
                maxOccurrences = std::max(maxOccurrences, it->second);
                it++;
            }
            if (totalOccurrences == 255) return;

            // Increment or decrement the max occurrence count to make the total be 255
            for (auto &[_, occurrences] : observations)
            {
                if (occurrences != maxOccurrences) continue;
                occurrences += (255 - totalOccurrences);
                return;
            }
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
                    ASSERT_GE(128, positionDeltas.size()) << "Positions delta exceeded limit";
                    positionDeltas.push_back(delta);
                    positionDeltaToIndex[delta] = (uint8_t)(positionDeltas.size() - 1);
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
                    gatherPositionDeltas(rootNode.pos, rootNode.nextPositions, positionDeltas, positionDeltaToIndex);
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

                    auto result =
                            (occurrenceOccumulator == 0)
                            ? 0
                            : ((unsigned int)std::round((double)delayAccumulator / (double)occurrenceOccumulator));
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

        MiningOptimizationV2::PositionAndVelocity convert(const PositionAndVelocity &pos)
        {
            return {pos.x, pos.y, pos.heading, (int16_t)pos.velocityX, (int16_t)pos.velocityY};
        }

        MiningOptimizationV2::PositionDeltaAndVelocity delta(const PositionAndVelocity &pos,
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
            // Start by looking up the next path arrival delay, defaulting to the max value if it isn't found
            auto it = nextPathArrivalDelays.find(arrivalData.nextPathStartPosition);
            uint8_t nextPathArrivalDelay = (it == nextPathArrivalDelays.end()) ? 63 : it->second;

            // Now pack the next path arrival delay with whatever is already packed in the arrival data
            // For gather this is the collision and facing patch, for return this is the exit speed
            uint8_t packed = (nextPathArrivalDelay << 2) + (arrivalData.packed & 0b00000011);

            // Validate the arrival delay can fit in 8 bits
            EXPECT_LE(arrivalData.arrivalDelay(), 255) << "Arrival delay does not fit in 8 bits";

            return {(uint8_t)arrivalData.arrivalDelay(), packed};
        }

        template <typename TrainingObservationType, typename OutputObservationType>
        std::vector<std::pair<MiningOptimizationV2::PathNode<OutputObservationType>, uint8_t>> convert( // NOLINT(*-no-recursion)
                const PositionAndVelocity &pos,
                const std::vector<std::pair<PathNode<TrainingObservationType>, uint32_t>> &nextNodes,
                const std::map<std::pair<int8_t, int8_t>, uint8_t> &positionDeltaToIndex,
                const std::unordered_map<PositionAndVelocity, uint8_t> &nextPathArrivalDelays)
        {
            if (nextNodes.empty()) return {};

            auto convertArrivalData = [&](const std::map<TrainingObservationType, uint32_t> &observations) -> std::map<OutputObservationType, uint8_t>
            {
                if (observations.empty()) return {};

                std::unique_ptr<uint32_t> totalArrivalOccurrences;
                std::map<OutputObservationType, uint8_t> result;
                for (const auto &[arrivalData, occurrences] : observations)
                {
                    result.emplace(convert<TrainingObservationType, OutputObservationType>(arrivalData, nextPathArrivalDelays),
                                   computeOccurrenceRate(observations, totalArrivalOccurrences, occurrences));
                }

                ensureOccurrenceRateTotal(result);

                return result;
            };

            auto convertNode =
                    [&](const PathNode<TrainingObservationType> &node) // NOLINT(*-no-recursion)
                    -> MiningOptimizationV2::PathNode<OutputObservationType>
            {
                // Assert that if non-resend data is unavailable, resend data is also unavailable
                if (node.arrivalData.empty())
                {
                    EXPECT_TRUE(node.arrivalDataAfterResend.empty()) << "Empty arrivalData should mean empty arrivalDataAfterResend";
                }
                if (node.nextPositions.empty())
                {
                    EXPECT_TRUE(node.nextPositionsAfterResend.empty()) << "Empty nextPositions should mean empty nextPositionsAfterResend";
                }

                return {delta(pos, node.pos, positionDeltaToIndex),
                        convertArrivalData(node.arrivalData),
                        convertArrivalData(node.arrivalDataAfterResend),
                        convert<TrainingObservationType, OutputObservationType>(node.pos, node.nextPositions, positionDeltaToIndex, nextPathArrivalDelays),
                        convert<TrainingObservationType, OutputObservationType>(node.pos, node.nextPositionsAfterResend, positionDeltaToIndex, nextPathArrivalDelays)};
            };

            std::unique_ptr<uint32_t> totalNodeOccurrences;
            std::vector<std::pair<MiningOptimizationV2::PathNode<OutputObservationType>, uint8_t>> result;
            result.reserve(nextNodes.size());
            for (const auto &[node, occurrences] : nextNodes)
            {
                result.emplace_back(convertNode(node), computeOccurrenceRate(nextNodes, totalNodeOccurrences, occurrences));
            }

            ensureOccurrenceRateTotal(result);

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
        MiningOptimizationV2::Path<OutputObservationType> convert(const Path<TrainingObservationType> &rootNode,
                                                                  const std::map<std::pair<int8_t, int8_t>, uint8_t> &positionDeltaToIndex,
                                                                  const std::unordered_map<PositionAndVelocity, uint8_t> &nextPathArrivalDelays)
        {
            return {convert(rootNode.pos),
                    convert<TrainingObservationType, OutputObservationType>(rootNode.pos,
                                                                            rootNode.nextPositions,
                                                                            positionDeltaToIndex,
                                                                            nextPathArrivalDelays)};
        }

        template <typename TrainingObservationType, typename OutputObservationType>
        void transform(
                const std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, Path<TrainingObservationType>>> &pathData,
                std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, uint8_t>> &nextPathArrivalDelays,
                std::unordered_map<TilePosition, std::unordered_map<MiningOptimizationV2::PositionAndVelocity,
                                                                    MiningOptimizationV2::Path<OutputObservationType>>> &outputData,
                const std::map<std::pair<int8_t, int8_t>, uint8_t> &positionDeltaToIndex)
        {
            for (const auto &[tile, rootNodes] : pathData)
            {
                // Compute the arrival delays for the next paths
                auto &patchNextPathArrivalDelays = nextPathArrivalDelays[tile];

                // Convert the root nodes
                auto &outputRootNodes = outputData[tile];
                for (const auto &[pos, rootNode] : rootNodes)
                {
                    outputRootNodes[convert(pos)] =
                            convert<TrainingObservationType, OutputObservationType>(rootNode, positionDeltaToIndex, patchNextPathArrivalDelays);
                }
            }
        }
    }

    void transform(const MapData &trainingData)
    {
        MiningOptimizationV2::MapData outputData;
        outputData.mapHash = trainingData.mapHash;

        // Start by finding all of the needed position deltas
        std::cout << "Building position delta map..." << std::endl;
        std::map<std::pair<int8_t, int8_t>, uint8_t> positionDeltaToIndex;
        gatherPositionDeltas(trainingData.resourceToGatherPaths, outputData.positionDeltas, positionDeltaToIndex);
        gatherPositionDeltas(trainingData.resourceToReturnPaths, outputData.positionDeltas, positionDeltaToIndex);
        std::cout << "...found " << outputData.positionDeltas.size() << " position deltas" << std::endl;

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
        transform(trainingData.resourceToGatherPaths, returnAverageArrivalDelays, outputData.resourceToGatherPaths, positionDeltaToIndex);
        std::cout << "...done!" << std::endl;

        std::cout << "Transforming return data..." << std::endl;
        transform(trainingData.resourceToReturnPaths, gatherAverageArrivalDelays, outputData.resourceToReturnPaths, positionDeltaToIndex);
        std::cout << "...done!" << std::endl;

        // Finally serialize everything
        std::cout << "Serializing map data..." << std::endl;
        MiningOptimizationV2::Serialization::writeMapData(outputData);
        std::cout << "...done!" << std::endl;
    }
}
