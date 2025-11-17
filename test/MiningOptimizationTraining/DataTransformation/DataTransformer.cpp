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
        std::map<unsigned int, unsigned long> arrivalDataCounts;
        std::map<unsigned int, unsigned long> nextPositionCounts;
        unsigned long totalNextNodes = 0;
        unsigned long nextNodePositionDeltaCollisions = 0;
        unsigned long nextNodePositionAndVelocityCollisions = 0;

        template <typename ObservationType>
        void processNextNodes(
                PatchData &patchData,
                const PositionAndVelocity &pos,
                const std::vector<std::pair<PathNode<ObservationType>, uint32_t>> &nextNodes)
        {
            uint32_t totalOccurrences = 0;
            for (const auto &[_, occurrences] : nextNodes) totalOccurrences += occurrences;

            std::set<PositionAndVelocity> positionsAndVelocities;
            std::set<std::pair<int8_t, int8_t>> positionDeltas;

            size_t nodeCount = 0;
            for (const auto &[nextNode, occurrences] : nextNodes)
            {
                nodeCount++;

                positionsAndVelocities.insert(nextNode.pos);

                auto delta = std::make_pair(nextNode.pos.x - pos.x, nextNode.pos.y - pos.y);
                positionDeltas.insert(delta);
                nextPositionDeltaCombinations.emplace(delta.first, delta.second);

                arrivalDataCounts[nextNode.arrivalData.size()]++;
                arrivalDataCounts[nextNode.arrivalDataAfterResend.size()]++;

                nextPositionCounts[nextNode.nextPositions.size()]++;
                nextPositionCounts[nextNode.nextPositionsAfterResend.size()]++;

                processNextNodes(patchData, nextNode.pos, nextNode.nextPositions);
                processNextNodes(patchData, nextNode.pos, nextNode.nextPositionsAfterResend);
            }

            totalNextNodes++;
            if (positionsAndVelocities.size() < nodeCount) nextNodePositionAndVelocityCollisions++;
            if (positionDeltas.size() < nodeCount) nextNodePositionDeltaCollisions++;
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

        template <typename ObservationType>
        void outputStatistics(const std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, Path<ObservationType>>> &pathData)
        {
            rootNodeHeadingAndVelocityCombinations.clear();
            nextPositionDeltaCombinations.clear();
            arrivalDataCounts.clear();
            nextPositionCounts.clear();
            totalNextNodes = 0;
            nextNodePositionDeltaCollisions = 0;
            nextNodePositionAndVelocityCollisions = 0;

            for (const auto &[tile, rootNodes] : pathData)
            {
                processPatch(rootNodes);
            }

            std::cout << std::fixed << std::setprecision(5)
                      << "rootNodeHeadingAndVelocityCombinations: " << rootNodeHeadingAndVelocityCombinations.size()
                      << "; bits: " << std::log2(rootNodeHeadingAndVelocityCombinations.size())
                      << std::endl;
            std::cout << std::fixed << std::setprecision(5)
                      << "nextPositionDeltaCombinations: " << nextPositionDeltaCombinations.size()
                      << "; bits: " << std::log2(nextPositionDeltaCombinations.size())
                      << std::endl;
            std::cout << "position delta collisions: " << nextNodePositionDeltaCollisions << " / " << totalNextNodes
                      << " (" << ((double)nextNodePositionDeltaCollisions * 100.0 / (double)totalNextNodes) << "%)"
                      << std::endl;
            std::cout << "position and velocity collisions: " << nextNodePositionAndVelocityCollisions << " / " << totalNextNodes
                      << " (" << ((double)nextNodePositionAndVelocityCollisions * 100.0 / (double)totalNextNodes) << "%)"
                      << std::endl;

            auto outCounts = [](const std::map<unsigned int, unsigned long> &counts)
            {
                unsigned long total = 0;
                for (const auto &[size, count] : counts)
                {
                    if (size == 0) continue;
                    total += count;
                }

                std::ostringstream out;
                out << std::fixed << std::setprecision(5);
                std::string sep;
                for (const auto &[size, count] : counts)
                {
                    if (size == 0) continue;
                    out << sep << size << ": " << count << " (" << ((double)count * 100.0 / (double)total) << "%)";
                    sep = "\n";
                }
                return out.str();
            };
            std::cout << "Arrival data counts: " << std::endl << outCounts(arrivalDataCounts) << std::endl;
            std::cout << "Next position counts: " << std::endl << outCounts(nextPositionCounts) << std::endl;
        }
    }

    void transform(const MapData &trainingData)
    {
        std::cout << "Gather:" << std::endl;
        outputStatistics(trainingData.resourceToGatherPaths);

        std::cout << std::endl << std::endl;

        std::cout << "Return:" << std::endl;
        outputStatistics(trainingData.resourceToReturnPaths);
    }
}
