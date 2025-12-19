#include "BWTest.h"

#include "Geo.h"

#include "MiningOptimizationTraining/DataModel/Serialization.h"

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
                    prune(rootNode.nextPositions);
                }
            }
        }

        for (const auto &[_, patchRootNodes] : rootNodes)
        {
            for (const auto &[_2, rootNode] : patchRootNodes)
            {
                explore((rootNode.timesExplored > WELL_EXPLORED_THRESHOLD) ? wellExploredResult : poorlyExploredResult,
                        rootNode.pos,
                        rootNode.pos,
                        rootNode.nextPositions);
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
    auto current = &path.nextPositions;
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

TEST(DataExploration, DeltasToTenDistance)
{
    MiningOptimizationTraining::MapData data;
    MiningOptimizationTraining::Serialization::setGameParameters(Maps::GetOne("Vermeer")->openbwHash);
    MiningOptimizationTraining::Serialization::readMapData(data);

    std::map<uint8_t, unsigned long> result;

    auto processArrivalData = [&](const std::map<GatherArrivalData, uint32_t> &arrivalDataMap)
    {
        for (const auto &arrivalData : arrivalDataMap)
        {
            result[arrivalData.first.tenDistanceDelta]++;
        }
    };

    std::function<void(const std::vector<std::pair<PathNode<GatherArrivalData>, uint32_t>>&)> processNextNodes;
    processNextNodes = [&](const std::vector<std::pair<PathNode<GatherArrivalData>, uint32_t>> &nextNodes)
    {
        for (const auto &node : nextNodes)
        {
            processArrivalData(node.first.arrivalData);
            processArrivalData(node.first.arrivalDataAfterResend);

            processNextNodes(node.first.nextPositions);
            processNextNodes(node.first.nextPositionsAfterResend);
        }
    };

    for (const auto &[_, gatherPaths] : data.resourceToGatherPaths)
    {
        for (const auto &[_, gatherPath] : gatherPaths)
        {
            processNextNodes(gatherPath.nextPositions);
        }
    }

    for (const auto &[delta, occurrences] : result)
    {
        std::cout << (unsigned int)delta << ": " << occurrences << std::endl;
    }
}
