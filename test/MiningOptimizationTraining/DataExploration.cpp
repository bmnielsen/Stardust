#include "BWTest.h"

#include "Geo.h"

#include "MiningOptimizationTraining/PathExploration/ExploreStartPositionsModule.h"
#include "ClearOpponentUnitsModule.h"

#include "MiningOptimizationTraining/DataModel/Serialization.h"

#include "MiningOptimizationV2/DataModel/Serialization.h"

// Root nodes explored above this value are considered "well-explored"
#define WELL_EXPLORED_THRESHOLD 3

// Prune nodes that have an occurrence rate lower than this (in percent)
#define PRUNE_THRESHOLD 10

using namespace MiningOptimizationTraining;

namespace
{
    struct Counter
    {
        // How many nodes have only one next node
        unsigned long stableNodes = 0;

        // How many nodes have more than one next node
        unsigned long unstableNodes = 0;

        // How many resend nodes can be taken without spanning an unstable node within LF
        unsigned long stableResendNodes = 0;

        // How many resend nodes cannot be taken without spanning an unstable node within LF
        unsigned long unstableResendNodes = 0;

        friend std::ostream& operator<< (std::ostream& os, const Counter& counter)
        {
            auto totalNodes = counter.stableNodes + counter.unstableNodes;
            auto totalResendNodes = counter.stableResendNodes + counter.unstableResendNodes;

            std::ostringstream out;
            out << std::fixed << std::setprecision(2);

            out << "Nodes with next positions: " << totalNodes << std::endl;
            out << " Stable nodes: " << ((double)counter.stableNodes * 100.0 / (double)totalNodes) << "%" << std::endl;
            out << " Unstable nodes: " << ((double)counter.unstableNodes * 100.0 / (double)totalNodes) << "%" << std::endl;
            out << "Resend nodes: " << totalResendNodes << std::endl;
            out << " Stable resend nodes: " << ((double)counter.stableResendNodes * 100.0 / (double)totalResendNodes) << "%" << std::endl;
            out << " Unstable resend nodes: " << ((double)counter.unstableResendNodes * 100.0 / (double)totalResendNodes) << "%" << std::endl;

            os << out.str();
            return os;
        }
    };

    template <typename ObservationType>
    void prune(std::vector<std::pair<PathNode<ObservationType>, uint32_t>> &nextNodes)
    {
        if (nextNodes.size() > 1)
        {
            unsigned long totalOccurrences = 0;
            for (const auto &[_, occurrences] : nextNodes) totalOccurrences += occurrences;

            unsigned long threshold = (totalOccurrences * PRUNE_THRESHOLD) / 100;
            for (auto it = nextNodes.begin(); it != nextNodes.end(); )
            {
                if (it->second > threshold)
                {
                    it++;
                }
                else
                {
                    it = nextNodes.erase(it);
                }
            }
        }

        for (auto &[node, _] : nextNodes)
        {
            prune(node.nextPositions);
            prune(node.nextPositionsAfterResend);
        }
    }

    template <typename ObservationType>
    void explore(Counter &result,
                 const PositionAndVelocity &pathStartPosition,
                 const PositionAndVelocity &nodePosition,
                 const std::vector<std::pair<PathNode<ObservationType>, uint32_t>> &nextNodes,
                 bool isResendNode = false,
                 int stepsSinceUnstableNode = 3)
    {
        if (nextNodes.empty()) return;

        // Record results for this node only if it comes after the worker moved from the start position
        if (Geo::ApproximateDistance(pathStartPosition.x, nodePosition.x, pathStartPosition.y, nodePosition.y) > 1)
        {
            if (nextNodes.size() == 1)
            {
                result.stableNodes++;
            }
            else
            {
                result.unstableNodes++;
            }

            if (isResendNode)
            {
                if (stepsSinceUnstableNode >= 3)
                {
                    result.stableResendNodes++;
                }
                else
                {
                    result.unstableResendNodes++;
                }
            }
        }

        int nextStepsSinceUnstableNode = (nextNodes.size() > 1) ? 0 : (stepsSinceUnstableNode + 1);
        for (const auto &[node, _] : nextNodes)
        {
            if (node.type == NodeType::Uninitialized || node.type == NodeType::AfterExplorationWindow) continue;
            explore(result,
                    pathStartPosition,
                    node.pos,
                    node.nextPositions,
                    node.type != NodeType::PoorResendNode && node.type != NodeType::ResendUnavailable,
                    nextStepsSinceUnstableNode);
            explore(result,
                    pathStartPosition,
                    node.pos,
                    node.nextPositionsAfterResend,
                    node.type != NodeType::PoorResendNode && node.type != NodeType::ResendUnavailable,
                    nextStepsSinceUnstableNode);
        }
    }

    template <typename ObservationType>
    void explore(Counter &wellExploredResult,
                 Counter &poorlyExploredResult,
                 std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, Path<ObservationType>>> &rootNodes)
    {
        if constexpr (PRUNE_THRESHOLD > 0)
        {
            for (auto &[_, patchRootNodes] : rootNodes)
            {
                for (auto &[_2, rootNode] : patchRootNodes)
                {
                    prune(rootNode.nextPositions[{}]);
                }
            }
        }

        for (const auto &[_, patchRootNodes] : rootNodes)
        {
            for (const auto &[_2, rootNode] : patchRootNodes)
            {
                explore(((rootNode.timesExploredWithNoCollision + rootNode.timesExploredWithCollision) > WELL_EXPLORED_THRESHOLD)
                            ? wellExploredResult
                            : poorlyExploredResult,
                        rootNode.pos,
                        rootNode.pos,
                        rootNode.nextPositions.at({}));
            }
        }
    }

    void explore(const std::string &mapHash)
    {
        MiningOptimizationTraining::MapData data;

        MiningOptimizationTraining::Serialization::setGameParameters(mapHash);
        MiningOptimizationTraining::Serialization::readMapData(data);

        Counter gatherWellExplored, gatherPoorlyExplored, returnWellExplored, returnPoorlyExplored;
        explore(gatherWellExplored, gatherPoorlyExplored, data.resourceToGatherPaths);
        explore(returnWellExplored, returnPoorlyExplored, data.resourceToReturnPaths);

        std::cout << "Gather well explored:" << std::endl << gatherWellExplored << std::endl;
        std::cout << "Gather poorly explored:" << std::endl << gatherPoorlyExplored << std::endl;
        std::cout << "Return well explored:" << std::endl << returnWellExplored << std::endl;
        std::cout << "Return poorly explored:" << std::endl << returnPoorlyExplored << std::endl;
    }
}

TEST(DataExploration, Vermeer)
{
    explore(Maps::GetOne("Vermeer")->openbwHash);
}

TEST(DataExploration, SimulateSpecificPath)
{
    ExploreStartPositionsModuleOptions options;
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    test.opponentRace = BWAPI::Races::Terran;
    test.opponentModule = []()
    {
        return new ClearOpponentUnitsModule();
    };
    test.myModule = [&]()
    {
        return new ExploreStartPositionsModule<SimulateSpecificPath>(options);
    };
    test.allowOpponentOutput = false;
    test.expectWin = false;
    test.randomSeed = 42;
    test.writeReplay = false;
    test.frameLimit = 100;
    test.run();
}

TEST(DataExploration, SimulateAllSubpixelsOfPosition)
{
    ExploreStartPositionsModuleOptions options;
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    test.opponentRace = BWAPI::Races::Terran;
    test.opponentModule = []()
    {
        return new ClearOpponentUnitsModule();
    };
    test.myModule = [&]()
    {
        return new ExploreStartPositionsModule<SimulateAllSubpixelsOfPosition>(options);
    };
    test.allowOpponentOutput = false;
    test.expectWin = false;
    test.randomSeed = 42;
    test.writeReplay = false;
    test.frameLimit = 100;
    test.run();
}

TEST(DataExploration, CheckSpecificPath)
{
    MiningOptimizationTraining::MapData data;
    MiningOptimizationTraining::Serialization::setGameParameters(Maps::GetOne("Vermeer")->openbwHash);
    MiningOptimizationTraining::Serialization::readMapData(data);

    PositionAndVelocity startPos{
            56320/256,95232/256,-95,0,0
    };
    auto &path = data.resourceToReturnPaths.at(TilePosition(5, 12)).at(startPos);

    PositionAndVelocity resendPos{
            56366/256,80216/256,2,0,-1280
    };
    auto current = &path.nextPositions[{}];
    while (current)
    {
        if (current->empty())
        {
            std::cout << "Path end" << std::endl;
            return;
        }
        if (current->size() > 1)
        {
            std::cout << "Unstable path" << std::endl;
            return;
        }

        auto &node = current->begin()->first;
        if (node.pos == resendPos)
        {
            std::cout << "Found it" << std::endl;
            return;
        }
        current = &node.nextPositions;
    }
}

TEST(DataExploration, DeltasToTenDistanceAndResendAlwaysArrives)
{
    MiningOptimizationTraining::MapData data;
    MiningOptimizationTraining::Serialization::setGameParameters(Maps::GetOne("Vermeer")->openbwHash);
    MiningOptimizationTraining::Serialization::readMapData(data);

    std::map<uint8_t, unsigned long> tenDistanceResult;
    std::map<uint8_t, unsigned long> resendAlwaysArrivesResult;
    std::map<std::pair<uint8_t, uint8_t>, unsigned long> combinedResult;

    auto processArrivalData = [&](const std::map<GatherArrivalData, uint32_t> &arrivalDataMap)
    {
        for (const auto &arrivalData : arrivalDataMap)
        {
            tenDistanceResult[arrivalData.first.tenDistanceDelta]++;
            resendAlwaysArrivesResult[arrivalData.first.resendAlwaysArrivesDelta]++;
            combinedResult[std::make_pair(arrivalData.first.tenDistanceDelta, arrivalData.first.resendAlwaysArrivesDelta)]++;
        }
    };

    std::function<void(const std::vector<std::pair<PathNode<GatherArrivalData>, uint32_t>>&)> processNextNodes;
    processNextNodes = [&](const std::vector<std::pair<PathNode<GatherArrivalData>, uint32_t>> &nextNodes)
    {
        for (const auto &node : nextNodes)
        {
            // This is only relevant for final resend nodes
            if (node.first.nextPositionsAfterResend.empty() && node.first.type != NodeType::PoorResendNode)
            {
                processArrivalData(node.first.arrivalDataAfterResend);
            }

            processNextNodes(node.first.nextPositions);
            processNextNodes(node.first.nextPositionsAfterResend);
        }
    };

    for (const auto &[_, gatherPaths] : data.resourceToGatherPaths)
    {
        for (const auto &[_, gatherPath] : gatherPaths)
        {
            processNextNodes(gatherPath.nextPositions.at({}));
        }
    }

    auto outMap = [](const std::map<uint8_t, unsigned long> &result)
    {
        for (const auto &[delta, occurrences] : result)
        {
            std::cout << (unsigned int)delta << ": " << occurrences << std::endl;
        }
    };
    std::cout << "Ten distance:" << std::endl;
    outMap(tenDistanceResult);
    std::cout << "Resend always arrives:" << std::endl;
    outMap(resendAlwaysArrivesResult);

    std::vector<std::pair<std::pair<uint8_t, uint8_t>, unsigned long>> sortedCombinedResults;
    sortedCombinedResults.reserve(combinedResult.size());
    for (const auto &item : combinedResult)
    {
        sortedCombinedResults.emplace_back(item);
    }
    std::sort(sortedCombinedResults.begin(), sortedCombinedResults.end(), [](const std::pair<std::pair<uint8_t, uint8_t>, unsigned long> &first,
                                                                             const std::pair<std::pair<uint8_t, uint8_t>, unsigned long> &second){
        return second.second < first.second;
    });
    std::cout << "Combined:" << std::endl;
    int i = 0;
    for (const auto &[pair, occurrences] : sortedCombinedResults)
    {
        std::cout << i << ": (" << (unsigned int)pair.first << "," << (unsigned int)pair.second << "): " << occurrences << std::endl;
        i++;
    }
}

TEST(DataExploration, GatherPathPruning)
{
    MiningOptimizationTraining::MapData data;
    MiningOptimizationTraining::Serialization::setGameParameters(Maps::GetOne("Vermeer")->openbwHash);
    MiningOptimizationTraining::Serialization::readMapData(data);

    size_t maxCount = 0;
    TilePosition bestTile = {0, 0};
    for (const auto &[tile, paths] : data.resourceToGatherPaths)
    {
        if (paths.size() > maxCount)
        {
            maxCount = paths.size();
            bestTile = tile;
        }
    }

    const auto &mostPaths = data.resourceToGatherPaths[bestTile];
    std::cout << "Most gather paths is " << mostPaths.size() << std::endl;


}

TEST(DataExploration, ReturnCollisions)
{
    MiningOptimizationTraining::MapData data;
    MiningOptimizationTraining::Serialization::setGameParameters(Maps::GetOne("Vermeer")->openbwHash);
    MiningOptimizationTraining::Serialization::readMapData(data);

    std::map<bool, unsigned long> deliveryAtArrivalCollisions;
    std::map<bool, unsigned long> deliveryAfterArrivalCollisions;

    auto processArrivalData = [&](const std::map<ReturnArrivalData, uint32_t> &arrivalDataMap)
    {
        for (const auto &[arrivalData, _] : arrivalDataMap)
        {
            deliveryAtArrivalCollisions[arrivalData.exitSpeedDeliveryAtArrival == 0]++;
            deliveryAfterArrivalCollisions[arrivalData.collisionDeliveryAfterArrival]++;
        }
    };

    std::function<void(const std::vector<std::pair<PathNode<ReturnArrivalData>, uint32_t>>&)> processNextNodes;
    processNextNodes = [&](const std::vector<std::pair<PathNode<ReturnArrivalData>, uint32_t>> &nextNodes)
    {
        for (const auto &node : nextNodes)
        {
            // This is only relevant for final nodes
            if (node.first.nextPositions.empty())
            {
                processArrivalData(node.first.arrivalData);
            }
            else if (node.first.nextPositionsAfterResend.empty() && node.first.type != NodeType::PoorResendNode)
            {
                processArrivalData(node.first.arrivalDataAfterResend);
            }

            processNextNodes(node.first.nextPositions);
            processNextNodes(node.first.nextPositionsAfterResend);
        }
    };

    for (const auto &[_, returnPaths] : data.resourceToReturnPaths)
    {
        for (const auto &[_, returnPath] : returnPaths)
        {
            processNextNodes(returnPath.nextPositions.at({}));
        }
    }

    auto outMap = [](std::map<bool, unsigned long> &map)
    {
        std::cout << "true: " << map[true] << std::endl;
        std::cout << "false: " << map[false] << std::endl;
    };
    std::cout << "At arrival: " << std::endl;
    outMap(deliveryAtArrivalCollisions);
    std::cout << "After arrival: " << std::endl;
    outMap(deliveryAfterArrivalCollisions);
}

TEST(DataExploration, ReturnCollisionsProductionData)
{
    MiningOptimization::MapData data;
    MiningOptimization::Serialization::setGameParameters(Maps::GetOne("Vermeer")->openbwHash);
    MiningOptimization::Serialization::readMapData(data);

    std::map<bool, unsigned long> deliveryAtArrivalCollisions;
    std::map<bool, unsigned long> deliveryAfterArrivalCollisions;

    auto processArrivalData = [&](const std::vector<std::pair<MiningOptimization::ReturnArrivalData, uint8_t>> &arrivalDataMap)
    {
        for (const auto &[arrivalData, _] : arrivalDataMap)
        {
            deliveryAtArrivalCollisions[arrivalData.exitSpeed() == 0]++;
            deliveryAfterArrivalCollisions[arrivalData.collision()]++;
        }
    };

    std::function<void(const std::vector<std::pair<MiningOptimization::PathNode<MiningOptimization::ReturnArrivalData>, uint8_t>>&)> processNextNodes;
    processNextNodes = [&](const std::vector<std::pair<MiningOptimization::PathNode<MiningOptimization::ReturnArrivalData>, uint8_t>> &nextNodes)
    {
        for (const auto &[node, _] : nextNodes)
        {
            // This is only relevant for final nodes
            if (node.nextPositions.empty())
            {
                processArrivalData(node.arrivalDataWhenFinalNode);
            }
            else if (node.nextPositionsAfterResend.empty())
            {
                processArrivalData(node.arrivalDataAfterResend);
            }

            processNextNodes(node.nextPositions);
            processNextNodes(node.nextPositionsAfterResend);
        }
    };

    for (const auto &[_, returnPaths] : data.resourceToSerializedReturnPaths)
    {
        for (const auto &[_, returnPath] : returnPaths)
        {
            auto deserializedPath = returnPath.get();
            processNextNodes(deserializedPath.nextPositions);
        }
    }

    auto outMap = [](std::map<bool, unsigned long> &map)
    {
        std::cout << "true: " << map[true] << std::endl;
        std::cout << "false: " << map[false] << std::endl;
    };
    std::cout << "At arrival: " << std::endl;
    outMap(deliveryAtArrivalCollisions);
    std::cout << "After arrival: " << std::endl;
    outMap(deliveryAfterArrivalCollisions);
}

TEST(DataExploration, ReturnStartPositionsByPatch)
{
    MiningOptimizationTraining::MapData data;
    MiningOptimizationTraining::Serialization::setGameParameters(Maps::GetOne("Vermeer")->openbwHash);
    MiningOptimizationTraining::Serialization::readMapData(data);

    unsigned long total = 0;
    for (auto &[patchTile, returnPaths] : data.resourceToReturnPaths)
    {
        total += returnPaths.size();
        for (auto &[pos, _] : returnPaths)
        {
            std::cout << patchTile << ": " << pos << std::endl;
        }
    }

    std::cout << "Total: " << total << std::endl;
}

TEST(DataExploration, FindMaintainedSpeedEffect)
{
    MapData data;
    Serialization::setGameParameters(Maps::GetOne("Benzene")->openbwHash);
    Serialization::readMapData(data);

    std::map<uint8_t, unsigned long> counters;
    std::map<uint8_t, int> accumulators;
    for (const auto &[patch, returnPaths] : data.resourceToReturnPaths)
    {
        const auto &gatherPaths = data.resourceToGatherPaths[patch];
        auto gatherPathLength = [&gatherPaths](const PositionAndVelocity &pos)
        {
            auto it = gatherPaths.find(pos);
            EXPECT_NE(gatherPaths.end(), it) << "Could not find gather path for " << pos;
            return it->second.nextPositions.begin()->second.begin()->first.arrivalData.begin()->first.arrivalDelay();
        };

        for (const auto &[_, returnPath] : returnPaths)
        {
            for (const auto &[_, rootNodes] : returnPath.nextPositions)
            {
                for (const auto &[rootNode, _] : rootNodes)
                {
                    for (const auto &[arrivalData, _] : rootNode.arrivalData)
                    {
                        if (arrivalData.exitSpeedDeliveryAtArrival == 0) continue;

                        counters[arrivalData.exitSpeedDeliveryAtArrival]++;

                        int benefit = gatherPathLength(arrivalData.nextPathStartPositionDeliveryAfterArrival) -
                            gatherPathLength(arrivalData.nextPathStartPositionDeliveryAtArrival);
                        accumulators[arrivalData.exitSpeedDeliveryAtArrival] += benefit;
                    }
                }
            }
        }
    }

    for (const auto &[exitSpeed, count] : counters)
    {
        double benefit = (double)accumulators[exitSpeed] / (double)count;
        std::cout << std::fixed << std::setprecision(2) << (unsigned int)exitSpeed << ": " << benefit << " (over " << count << " samples)" << std::endl;
    }
}

TEST(DataExploration, FindDefaultTakeoverMetadata)
{
    MapData data;
    Serialization::setGameParameters(Maps::GetOne("Benzene")->openbwHash);
    Serialization::readMapData(data);

    auto distAndSpeedAtDelta = [](TilePosition patchTile, const GatherPathNode &node, unsigned int delta)
    {
        auto current = &node;
        while (true)
        {
            if (current->arrivalData.begin()->first.arrivalDelay() <= delta)
            {
                auto dist = Geo::EdgeToEdgeDistance(
                    BWAPI::UnitTypes::Protoss_Probe,
                    current->pos,
                    BWAPI::UnitTypes::Resource_Mineral_Field,
                    patchTile);

                double dx = (double)current->pos.velocityX / 256.0;
                double dy = (double)current->pos.velocityY / 256.0;
                auto speed = (unsigned int)std::round(std::sqrt(dx * dx + dy * dy));

                return std::make_pair(dist, speed);
            }
            current = &current->nextPositions.begin()->first;
        }
    };

    std::map<std::pair<int, unsigned int>, unsigned long> tenDistanceCounters;
    std::map<std::pair<int, unsigned int>, unsigned long> resendAlwaysArrivesCounters;
    for (const auto &[patch, gatherPaths] : data.resourceToGatherPaths)
    {
        for (const auto &[_, gatherPath] : gatherPaths)
        {
            for (const auto &[_, rootNodes] : gatherPath.nextPositions)
            {
                for (const auto &[rootNode, _] : rootNodes)
                {
                    for (const auto &[arrivalData, _] : rootNode.arrivalData)
                    {
                        if (arrivalData.tenDistanceDelta != 255)
                        {
                            auto tenDistance = distAndSpeedAtDelta(patch, rootNode, arrivalData.tenDistanceDelta + 3);
                            tenDistanceCounters[tenDistance]++;
                        }
                        if (arrivalData.resendAlwaysArrivesDelta != 255)
                        {
                            auto resendAlwaysArrives = distAndSpeedAtDelta(patch, rootNode, arrivalData.resendAlwaysArrivesDelta + 3);
                            resendAlwaysArrivesCounters[resendAlwaysArrives]++;
                        }
                    }
                }
            }
        }
    }

    auto out = [](const std::map<std::pair<int, unsigned int>, unsigned long> &map)
    {
        for (const auto &[pair, count] : map)
        {
            std::cout << pair.first << " - " << pair.second << ": " << count << std::endl;
        }
    };

    std::cout << "10 distance: " << std::endl;
    out(tenDistanceCounters);
    std::cout << "resend always arrives: " << std::endl;
    out(resendAlwaysArrivesCounters);
}
