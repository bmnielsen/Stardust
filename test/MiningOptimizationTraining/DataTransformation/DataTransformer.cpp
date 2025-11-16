#include "DataTransformer.h"

namespace MiningOptimizationTraining::DataTransformer
{
    namespace
    {
        struct PatchData
        {
        };

        std::set<std::tuple<int8_t, int32_t, int32_t>> rootNodeHeadingAndVelocityCombinations;
        std::set<std::tuple<int, int>> nextPositionDeltaCombinations;

        template <typename ObservationType>
        void processNextNodes(
                PatchData &patchData,
                const PositionAndVelocity &pos,
                const std::vector<std::pair<PathNode<ObservationType>, uint32_t>> &nextNodes)
        {
            for (const auto &[nextNode, _] : nextNodes)
            {
                nextPositionDeltaCombinations.emplace(nextNode.pos.x - pos.x, nextNode.pos.y - pos.y);
                processNextNodes(patchData, nextNode.pos, nextNode.nextPositions);
                processNextNodes(patchData, nextNode.pos, nextNode.nextPositionsAfterResend);
            }
        }

        template <typename ObservationType>
        void processPatch(const std::unordered_map<PositionAndVelocity, Path<ObservationType>> &rootNodes)
        {
            PatchData patchData;
            for (const auto &[_, rootNode] : rootNodes)
            {
                rootNodeHeadingAndVelocityCombinations.emplace(rootNode.pos.heading, rootNode.pos.velocityX, rootNode.pos.velocityY);
                processNextNodes(patchData, rootNode.pos, rootNode.nextPositions);
            }
        }
    }

    void transform(const MapData &trainingData)
    {
        for (const auto &[tile, rootNodes] : trainingData.resourceToGatherPaths)
        {
            processPatch(rootNodes);
        }

        std::cout << "rootNodeHeadingAndVelocityCombinations: " << rootNodeHeadingAndVelocityCombinations.size()
                  << "; bits: " << std::log2(rootNodeHeadingAndVelocityCombinations.size())
                  << std::endl;
        std::cout << "nextPositionDeltaCombinations: " << nextPositionDeltaCombinations.size()
                  << "; bits: " << std::log2(nextPositionDeltaCombinations.size())
                  << std::endl;
    }
}
