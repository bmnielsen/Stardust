#include "DataTransformer.h"

#include "InitialSplitSolver.h"
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
                                      const std::map<std::pair<int8_t, int8_t>, uint8_t> &tenDistanceAndResendAlwaysArrivesToIndex)
        {
            return convert<TrainingObservationType, OutputObservationType>(arrivalData);
        }

        template <>
        MiningOptimization::GatherArrivalData convert(const GatherArrivalData &arrivalData,
                                                      const std::map<std::pair<int8_t, int8_t>, uint8_t> &tenDistanceAndResendAlwaysArrivesToIndex)
        {
            uint8_t packed = (std::min(arrivalData.arrivalDelay(), 63U) << 2) + (arrivalData.packed & 0b00000011);
            MiningOptimization::GatherArrivalData result{packed};

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
                                                      const std::map<std::pair<int8_t, int8_t>, uint8_t> &tenDistanceAndResendAlwaysArrivesToIndex)
        {
            return MiningOptimization::ReturnArrivalData{
                ReturnArrivalData::pack(arrivalData.arrivalDelay, arrivalData.collisionDeliveryAfterArrival),
                ReturnArrivalData::pack(arrivalData.exitSpeedDeliveryAtArrival, arrivalData.facingDepot),
            };
        }

        template <typename TrainingObservationType, typename OutputObservationType>
        std::vector<std::pair<MiningOptimization::PathNode<OutputObservationType>, uint8_t>> convert( // NOLINT(*-no-recursion)
                const PositionAndVelocity &pos,
                const std::vector<std::pair<PathNode<TrainingObservationType>, uint32_t>> &nextNodes,
                const std::map<std::pair<int8_t, int8_t>, uint8_t> &positionDeltaToIndex,
                const std::map<std::pair<int8_t, int8_t>, uint8_t> &tenDistanceAndResendAlwaysArrivesToIndex)
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
                                                                                             tenDistanceAndResendAlwaysArrivesToIndex);
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
                    default:
                        EXPECT_FALSE(true) << "Unknown node type encountered";
                }

                // The arrival data is only needed when this is the final node
                // We convert all of them here for simplicity, and they are then excluded for other nodes in the serialization
                // To ensure data consistency we do some validations here
                if (node.nextPositions.empty() && node.type != NodeType::Test)
                {
                    EXPECT_FALSE(node.arrivalData.empty());
                    for (const auto &[arrivalData, _] : node.arrivalData)
                    {
                        if constexpr (std::is_same_v<GatherArrivalData, TrainingObservationType>)
                        {
                            EXPECT_EQ(1, arrivalData.arrivalDelay());
                        }
                        else
                        {
                            EXPECT_EQ(1, arrivalData.arrivalDelay);
                        }
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
                                tenDistanceAndResendAlwaysArrivesToIndex),
                        convert<TrainingObservationType, OutputObservationType>(
                                node.pos,
                                node.nextPositionsAfterResend,
                                positionDeltaToIndex,
                                tenDistanceAndResendAlwaysArrivesToIndex),
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
                const std::map<std::pair<int8_t, int8_t>, uint8_t> &tenDistanceAndResendAlwaysArrivesToIndex)
        {
            std::map<MiningOptimization::CannonPlacement, MiningOptimization::Path<OutputObservationType>> result;

            for (auto &[cannonPlacement, nextPositions] : rootNode.nextPositions)
            {
                result[cannonPlacement] = {
                        convert(rootNode.pos),
                        convert<TrainingObservationType, OutputObservationType>(rootNode.pos,
                                                                                nextPositions,
                                                                                positionDeltaToIndex,
                                                                                tenDistanceAndResendAlwaysArrivesToIndex)};
            }

            return result;
        }

        template <typename TrainingObservationType, typename OutputObservationType>
        void transform(
                const std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, Path<TrainingObservationType>>> &pathData,
                std::unordered_map<TilePosition, std::unordered_map<MiningOptimization::PositionAndVelocity,
                                                                    MiningOptimization::SerializedPath<OutputObservationType>>> &outputData,
                const std::map<std::pair<int8_t, int8_t>, uint8_t> &positionDeltaToIndex,
                const std::map<std::pair<int8_t, int8_t>, uint8_t> &tenDistanceAndResendAlwaysArrivesToIndex)
        {
            for (const auto &[tile, rootNodes] : pathData)
            {
                // Convert the root nodes
                auto &outputRootNodes = outputData[tile];
                for (const auto &[pos, rootNode] : rootNodes)
                {
                    outputRootNodes[convert(pos)] = MiningOptimization::SerializedPath<OutputObservationType>::create(
                            convert<TrainingObservationType, OutputObservationType>(
                                    rootNode, positionDeltaToIndex, tenDistanceAndResendAlwaysArrivesToIndex));
                }
            }
        }
    }

    void transform(const MapData &trainingData, const InitialWorkerMapData &initialWorkerTrainingData)
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

        // Add a zero option first as a fallback
        outputData.tenDistanceAndResendAlwaysArrives.emplace_back(std::make_pair(0, 0));
        tenDistanceAndResendAlwaysArrivesToIndex[std::make_pair(0, 0)] = 0;

        unsigned long handledOccurrences = 0;
        unsigned long unhandledOccurrences = 0;
        for (int i = 0; i < sortedTenDistanceAndResendAlwaysArrivesToOccurrences.size(); i++)
        {
            if (i < 255)
            {
                outputData.tenDistanceAndResendAlwaysArrives.emplace_back(sortedTenDistanceAndResendAlwaysArrivesToOccurrences[i].first);
                tenDistanceAndResendAlwaysArrivesToIndex[sortedTenDistanceAndResendAlwaysArrivesToOccurrences[i].first] = (i + 1);
                handledOccurrences += sortedTenDistanceAndResendAlwaysArrivesToOccurrences[i].second;
            }
            else
            {
                unhandledOccurrences += sortedTenDistanceAndResendAlwaysArrivesToOccurrences[i].second;

                // Find the closest "safe" combination to use instead
                // This will cause the optimizer to be more careful, but gives a better result than not having anything
                auto key = sortedTenDistanceAndResendAlwaysArrivesToOccurrences[i].first;
                auto tryAssign = [&]()
                {
                    auto it = tenDistanceAndResendAlwaysArrivesToIndex.find(key);
                    if (it == tenDistanceAndResendAlwaysArrivesToIndex.end()) return false;

                    tenDistanceAndResendAlwaysArrivesToIndex[sortedTenDistanceAndResendAlwaysArrivesToOccurrences[i].first] = it->second;
                    return true;
                };
                while (true)
                {
                    if (key.first > 0)
                    {
                        --key.first;
                        if (tryAssign()) break;
                    }
                    if (key.second > 0)
                    {
                        --key.second;
                        if (tryAssign()) break;
                    }
                }
            }
        }
        std::cout << std::fixed << std::setprecision(2)
                  << "...found " << sortedTenDistanceAndResendAlwaysArrivesToOccurrences.size() << " combinations"
                  << "; " << handledOccurrences << " occurrences covered"
                  << "; " << unhandledOccurrences
                  << " (" << ((double)unhandledOccurrences * 100.0 / (double)(handledOccurrences + unhandledOccurrences)) << "%)"
                  << " occurrences not covered"
                  << std::endl;

        // Now transform the data
        std::cout << "Transforming gather data..." << std::endl;
        transform(trainingData.resourceToGatherPaths,
                  outputData.resourceToSerializedGatherPaths,
                  positionDeltaToIndex,
                  tenDistanceAndResendAlwaysArrivesToIndex);
        std::cout << "...done!" << std::endl;

        std::cout << "Transforming return data..." << std::endl;
        transform(trainingData.resourceToReturnPaths,
                  outputData.resourceToSerializedReturnPaths,
                  positionDeltaToIndex,
                  tenDistanceAndResendAlwaysArrivesToIndex);
        std::cout << "...done!" << std::endl;

        std::cout << "Running initial worker split solver..." << std::endl;
        for (const auto &[spawnPosition, _] : initialWorkerTrainingData.startingWorkerPositionToOrderProcessTimerReset)
        {
            auto position = MiningOptimization::PositionAndVelocity(
                spawnPosition.x,
                spawnPosition.y,
                0,
                0,
                0,
                false);
            auto trainingPosition = PositionAndVelocity(
                spawnPosition.x,
                spawnPosition.y,
                0,
                0,
                0);
            auto exactPosition = BWAPI::ExactPosition{
                (uint32_t)spawnPosition.x * 256,
                (uint32_t)spawnPosition.y * 256,
                0, 0, 0
            };

            std::vector<TilePosition> patches;
            for (int heading = INT8_MIN; heading <= INT8_MAX; heading += 8)
            {
                position.heading = (int8_t)heading;
                trainingPosition.heading = (int8_t)heading;
                exactPosition.heading = (int8_t)heading;
                if (!initialWorkerTrainingData.startingWorkerPositionToPatchToFirstGatherPath.contains(exactPosition)) continue;

                if (patches.empty())
                {
                    for (const auto &[tile, _]
                        : initialWorkerTrainingData.startingWorkerPositionToPatchToFirstGatherPath.at(exactPosition))
                    {
                        patches.emplace_back(tile);
                    }
                }

                auto &unknown = outputData.startLocationToPatchPairToInitialSplitDataUnknown[position];
                auto &zerg = outputData.startLocationToPatchPairToInitialSplitDataZerg[position];
                auto &notZerg = outputData.startLocationToPatchPairToInitialSplitDataNotZerg[position];

                for (const auto &firstPatch : patches)
                {
                    for (const auto &secondPatch : patches)
                    {
                        auto patchPair = std::make_pair(firstPatch, secondPatch);

                        auto runSolver = [&](BWAPI::Race race,
                                             std::map<std::pair<TilePosition, TilePosition>, MiningOptimization::InitialSplitData> &output)
                        {
                            auto result = InitialSplitSolver(
                                initialWorkerTrainingData,
                                trainingPosition,
                                firstPatch,
                                secondPatch,
                                race).execute();
                            if (result)
                            {
                                output.emplace(patchPair, std::move(*result));
                            }
                        };
                        runSolver(BWAPI::Races::Unknown, unknown);
                        runSolver(BWAPI::Races::Zerg, zerg);
                        runSolver(BWAPI::Races::Protoss, notZerg);
                    }
                }
            }
        }

        std::cout << "...done!" << std::endl;

        // Finally serialize everything
        std::cout << "Serializing map data..." << std::endl;
        MiningOptimization::Serialization::writeMapData(outputData);
        std::cout << "...done!" << std::endl;
    }
}
