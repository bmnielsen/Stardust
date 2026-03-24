#include "gtest/gtest.h"

#include "DataTransformer.h"
#include "MiningOptimizationV2/DataModel/Serialization.h"

TEST(DataTransformerTests, PositionDeltaIndex)
{
    MiningOptimizationTraining::MapData trainingData;
    trainingData.mapHash = "test";

    MiningOptimizationTraining::PositionAndVelocity rootPos(100, 100, 10, 10, 10);
    MiningOptimizationTraining::PositionAndVelocity childPos1(105, 100, 15, 15, -15);
    MiningOptimizationTraining::PositionAndVelocity childPos2(105, 100, 20, -20, 20);
    MiningOptimizationTraining::PositionAndVelocity childPos3(95, 100, 10, 10, 10);

    MiningOptimizationTraining::GatherPath gatherPath{rootPos};
    gatherPath.nextPositions.emplace_back(MiningOptimizationTraining::GatherPathNode{childPos1, MiningOptimizationTraining::NodeType::Test}, 1);
    gatherPath.nextPositions.emplace_back(MiningOptimizationTraining::GatherPathNode{childPos2, MiningOptimizationTraining::NodeType::Test}, 1);
    gatherPath.nextPositions.emplace_back(MiningOptimizationTraining::GatherPathNode{childPos3, MiningOptimizationTraining::NodeType::Test}, 1);

    std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::GatherPath> rootNodes;
    rootNodes.emplace(rootPos, std::move(gatherPath));

    trainingData.resourceToGatherPaths.emplace(TilePosition(0, 0), std::move(rootNodes));

    MiningOptimizationTraining::DataTransformer::transform(trainingData);

    MiningOptimization::MapData outputData;
    MiningOptimization::Serialization::setGameParameters("test");
    MiningOptimization::Serialization::readMapData(outputData);

    // Ensure the position deltas contain the expected values
    EXPECT_EQ(3, outputData.positionDeltas.size());
    uint8_t pos1And2Index = 255;
    uint8_t pos3Index = 255;
    for (uint8_t i = 0; i < (uint8_t)outputData.positionDeltas.size(); i++)
    {
        if (outputData.positionDeltas[i] == std::make_pair((int8_t)5, (int8_t)0)) pos1And2Index = i;
        if (outputData.positionDeltas[i] == std::make_pair((int8_t)-5, (int8_t)0)) pos3Index = i;
    }
    EXPECT_LT(pos1And2Index, 3);
    EXPECT_LT(pos3Index, 3);

    // Ensure the root node in the output data has the expected values
    MiningOptimization::PositionAndVelocity expectedRootPos(100, 100, 10, 10, 10, false);
    MiningOptimization::PositionDeltaAndVelocity expectedChildPos1((pos1And2Index << 1) + 1, 15, 15, -15);
    MiningOptimization::PositionDeltaAndVelocity expectedChildPos2((pos1And2Index << 1) + 1, 20, -20, 20);
    MiningOptimization::PositionDeltaAndVelocity expectedChildPos3((pos3Index << 1), 17, 58, -98); // heading and velocity don't matter here

    auto &outputPatchData = outputData.resourceToSerializedGatherPaths[TilePosition(0, 0)];
    EXPECT_TRUE(outputPatchData.contains(expectedRootPos));
    EXPECT_EQ(expectedRootPos, outputPatchData[expectedRootPos].pos);
    EXPECT_EQ(3, outputPatchData[expectedRootPos].get().nextPositions.size());
    EXPECT_EQ(expectedChildPos1, outputPatchData[expectedRootPos].get().nextPositions[0].first.pos);
    EXPECT_EQ(expectedChildPos2, outputPatchData[expectedRootPos].get().nextPositions[1].first.pos);
    EXPECT_EQ(expectedChildPos3, outputPatchData[expectedRootPos].get().nextPositions[2].first.pos);
}

TEST(DataTransformerTests, OccurrenceRounding)
{
    MiningOptimizationTraining::MapData trainingData;
    trainingData.mapHash = "test";

    MiningOptimizationTraining::PositionAndVelocity rootPos(100, 100, 10, 10, 10);
    MiningOptimizationTraining::PositionAndVelocity childPos1(105, 100, 15, 15, -15);
    MiningOptimizationTraining::PositionAndVelocity childPos2(105, 100, 20, -20, 20);
    MiningOptimizationTraining::PositionAndVelocity childPos3(95, 100, 10, 10, 10);
    MiningOptimizationTraining::PositionAndVelocity childPos4(95, 100, -16, 10, 10);
    MiningOptimizationTraining::PositionAndVelocity childPos5(101, 102, -16, 10, 10);
    MiningOptimizationTraining::PositionAndVelocity childPos6(102, 101, -16, 10, 10);
    MiningOptimizationTraining::PositionAndVelocity childPos7(103, 101, -16, 10, 10);

    MiningOptimizationTraining::GatherPath gatherPath1{rootPos};
    gatherPath1.nextPositions.emplace_back(MiningOptimizationTraining::GatherPathNode{childPos1, MiningOptimizationTraining::NodeType::Test}, 1);
    gatherPath1.nextPositions.emplace_back(MiningOptimizationTraining::GatherPathNode{childPos2, MiningOptimizationTraining::NodeType::Test}, 1);
    gatherPath1.nextPositions.emplace_back(MiningOptimizationTraining::GatherPathNode{childPos3, MiningOptimizationTraining::NodeType::Test}, 1);
    gatherPath1.nextPositions.emplace_back(MiningOptimizationTraining::GatherPathNode{childPos4, MiningOptimizationTraining::NodeType::Test}, 1);

    std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::GatherPath> rootNodes1;
    rootNodes1.emplace(rootPos, std::move(gatherPath1));

    trainingData.resourceToGatherPaths.emplace(TilePosition(0, 0), std::move(rootNodes1));

    MiningOptimizationTraining::GatherPath gatherPath2{rootPos};
    gatherPath2.nextPositions.emplace_back(MiningOptimizationTraining::GatherPathNode{childPos1, MiningOptimizationTraining::NodeType::Test}, 1);
    gatherPath2.nextPositions.emplace_back(MiningOptimizationTraining::GatherPathNode{childPos2, MiningOptimizationTraining::NodeType::Test}, 1);
    gatherPath2.nextPositions.emplace_back(MiningOptimizationTraining::GatherPathNode{childPos3, MiningOptimizationTraining::NodeType::Test}, 1);
    gatherPath2.nextPositions.emplace_back(MiningOptimizationTraining::GatherPathNode{childPos4, MiningOptimizationTraining::NodeType::Test}, 1);
    gatherPath2.nextPositions.emplace_back(MiningOptimizationTraining::GatherPathNode{childPos5, MiningOptimizationTraining::NodeType::Test}, 1);
    gatherPath2.nextPositions.emplace_back(MiningOptimizationTraining::GatherPathNode{childPos6, MiningOptimizationTraining::NodeType::Test}, 1);
    gatherPath2.nextPositions.emplace_back(MiningOptimizationTraining::GatherPathNode{childPos7, MiningOptimizationTraining::NodeType::Test}, 1);

    std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::GatherPath> rootNodes2;
    rootNodes2.emplace(rootPos, std::move(gatherPath2));

    trainingData.resourceToGatherPaths.emplace(TilePosition(1, 0), std::move(rootNodes2));

    MiningOptimizationTraining::DataTransformer::transform(trainingData);

    MiningOptimization::MapData outputData;
    MiningOptimization::Serialization::setGameParameters("test");
    MiningOptimization::Serialization::readMapData(outputData);

    MiningOptimization::PositionAndVelocity expectedRootPos(100, 100, 10, 10, 10, false);

    auto expectSum = [&](
            const std::unordered_map<MiningOptimization::PositionAndVelocity,
                    MiningOptimization::SerializedPath<MiningOptimization::GatherArrivalData>> &data)
    {
        EXPECT_TRUE(data.contains(expectedRootPos));
        unsigned int total = 0;
        for (const auto &[_, occurrenceRate] : data.at(expectedRootPos).get().nextPositions)
        {
            total += (unsigned int)occurrenceRate;
        }
        EXPECT_EQ(OCCURRENCE_SCALE, total);
    };

    expectSum(outputData.resourceToSerializedGatherPaths[TilePosition(0, 0)]);
    expectSum(outputData.resourceToSerializedGatherPaths[TilePosition(1, 0)]);
}

TEST(DataTransformerTests, GatherArrivalPacking)
{
    MiningOptimizationTraining::MapData trainingData;
    trainingData.mapHash = "test";

    // Set up the root node data for the return paths
    MiningOptimizationTraining::PositionAndVelocity rootPos1(100, 100, 10, 10, 10);
    MiningOptimizationTraining::PositionAndVelocity rootPos2(100, 101, 10, 10, 10);

    MiningOptimizationTraining::ReturnPath returnPath1{
            rootPos1, {}, 0, {},
            {{40, 1}, {50, 1}} // Average of 45
    };
    MiningOptimizationTraining::ReturnPath returnPath2{
            rootPos2, {}, 0, {},
            {{70, 1}, {80, 1}} // Average of 75, tests that we can store something normally outside of 6 bits
    };

    std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::ReturnPath> returnRootNodes;
    returnRootNodes.emplace(rootPos1, std::move(returnPath1));
    returnRootNodes.emplace(rootPos2, std::move(returnPath2));
    trainingData.resourceToReturnPaths.emplace(TilePosition(0, 0), std::move(returnRootNodes));

    // Set up the various arrival data we want to test
    MiningOptimizationTraining::PositionAndVelocity childPos(105, 100, 15, 15, -15);
    MiningOptimizationTraining::GatherArrivalData arrivalData1((63U << 2) + 3, 0, rootPos1);
    MiningOptimizationTraining::GatherArrivalData arrivalData2((63U << 2) + 2, 0, rootPos1);
    MiningOptimizationTraining::GatherArrivalData arrivalData3((63U << 2) + 1, 0, rootPos1);
    MiningOptimizationTraining::GatherArrivalData arrivalData4((63U << 2) + 0, 0, rootPos1);
    MiningOptimizationTraining::GatherArrivalData arrivalData5((10U << 2) + 3, 0, rootPos2);
    MiningOptimizationTraining::GatherArrivalData arrivalData6((20U << 2) + 2, 0, rootPos2);
    MiningOptimizationTraining::GatherArrivalData arrivalData7((30U << 2) + 1, 0, rootPos2);
    MiningOptimizationTraining::GatherArrivalData arrivalData8((40U << 2) + 0, 0, rootPos2);

    MiningOptimizationTraining::GatherPathNode gatherPathNode1{
            childPos,
            MiningOptimizationTraining::NodeType::Test,
            {},
            {{arrivalData1, 1}, {arrivalData2, 1}, {arrivalData3, 1}, {arrivalData4, 1}}};
    gatherPathNode1.nextPositions.emplace_back(
            MiningOptimizationTraining::GatherPathNode{childPos, MiningOptimizationTraining::NodeType::Test}, 1);

    MiningOptimizationTraining::GatherPath gatherPath1{rootPos1};
    gatherPath1.nextPositions.emplace_back(std::move(gatherPathNode1), 1);

    MiningOptimizationTraining::GatherPathNode gatherPathNode2{
            childPos,
            MiningOptimizationTraining::NodeType::Test,
            {},
            {{arrivalData5, 1}, {arrivalData6, 1}, {arrivalData7, 1}, {arrivalData8, 1}}};
    gatherPathNode2.nextPositions.emplace_back(
            MiningOptimizationTraining::GatherPathNode{childPos, MiningOptimizationTraining::NodeType::Test}, 1);

    MiningOptimizationTraining::GatherPath gatherPath2{rootPos2};
    gatherPath2.nextPositions.emplace_back(std::move(gatherPathNode2), 1);

    std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::GatherPath> rootNodes;
    rootNodes.emplace(rootPos1, std::move(gatherPath1));
    rootNodes.emplace(rootPos2, std::move(gatherPath2));

    trainingData.resourceToGatherPaths.emplace(TilePosition(0, 0), std::move(rootNodes));

    MiningOptimizationTraining::DataTransformer::transform(trainingData);

    MiningOptimization::MapData outputData;
    MiningOptimization::Serialization::setGameParameters("test");
    MiningOptimization::Serialization::readMapData(outputData);

    // Minimum next path length should be 45
    EXPECT_EQ(45, outputData.minimumNextPathLength);

    MiningOptimization::PositionAndVelocity expectedRootPos1(100, 100, 10, 10, 10, false);
    MiningOptimization::PositionAndVelocity expectedRootPos2(100, 101, 10, 10, 10, false);

    auto &outputPatchData = outputData.resourceToSerializedGatherPaths[TilePosition(0, 0)];
    auto validateRoot = [&](
            MiningOptimization::PositionAndVelocity rootPos,
            const std::vector<MiningOptimizationTraining::GatherArrivalData> &expectedArrivalData,
            unsigned int expectedNextPathLength)
    {
        EXPECT_TRUE(outputPatchData.contains(rootPos));
        auto nextPositions = outputPatchData[rootPos].get().nextPositions;
        EXPECT_EQ(1, nextPositions.size());
        auto &nextPosition = nextPositions[0].first;
        EXPECT_EQ(expectedArrivalData.size(), nextPosition.arrivalDataAfterResend.size());

        for (const auto &expectedArrival : expectedArrivalData)
        {
            bool found = false;
            for (const auto &[actualArrival, _] : nextPosition.arrivalDataAfterResend)
            {
                if (actualArrival.arrivalDelay() == expectedArrival.arrivalDelay()
                    && actualArrival.facingTarget() == expectedArrival.facingTarget()
                    && actualArrival.collision() == expectedArrival.collision()
#if USE_NEXT_PATH_LENGTHS
                    && actualArrival.nextPathLength(outputData.minimumNextPathLength) == expectedNextPathLength
#endif
                    )
                {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found);
        }
    };

    validateRoot(expectedRootPos1, {arrivalData1, arrivalData2, arrivalData3, arrivalData4}, 45);
    validateRoot(expectedRootPos2, {arrivalData5, arrivalData6, arrivalData7, arrivalData8}, 75);
}

TEST(DataTransformerTests, ReturnArrivalPacking)
{
    MiningOptimizationTraining::MapData trainingData;
    trainingData.mapHash = "test";

    // Set up the root node data for the gather paths
    MiningOptimizationTraining::PositionAndVelocity rootPos1(100, 100, 10, 10, 10);
    MiningOptimizationTraining::PositionAndVelocity rootPos2(100, 101, 10, 10, 10);
    MiningOptimizationTraining::GatherPath gatherPath1{
            rootPos1, {}, 0, {},
            {{40, 1}, {50, 1}} // Average of 45
    };
    MiningOptimizationTraining::GatherPath gatherPath2{
            rootPos2, {}, 0, {},
            {{70, 1}, {80, 1}} // Average of 75, tests that we can store something normally outside of 6 bits
    };

    std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::GatherPath> gatherRootNodes;
    gatherRootNodes.emplace(rootPos1, std::move(gatherPath1));
    gatherRootNodes.emplace(rootPos2, std::move(gatherPath2));
    trainingData.resourceToGatherPaths.emplace(TilePosition(0, 0), std::move(gatherRootNodes));

    // Set up the various arrival data we want to test
    MiningOptimizationTraining::PositionAndVelocity childPos(105, 100, 15, 15, -15);
    MiningOptimizationTraining::ReturnArrivalData arrivalData1((63U << 2) + 3, rootPos1);
    MiningOptimizationTraining::ReturnArrivalData arrivalData2((63U << 2) + 2, rootPos1);
    MiningOptimizationTraining::ReturnArrivalData arrivalData3((63U << 2) + 1, rootPos1);
    MiningOptimizationTraining::ReturnArrivalData arrivalData4((63U << 2) + 0, rootPos1);
    MiningOptimizationTraining::ReturnArrivalData arrivalData5((10U << 2) + 3, rootPos2);
    MiningOptimizationTraining::ReturnArrivalData arrivalData6((20U << 2) + 2, rootPos2);
    MiningOptimizationTraining::ReturnArrivalData arrivalData7((30U << 2) + 1, rootPos2);
    MiningOptimizationTraining::ReturnArrivalData arrivalData8((40U << 2) + 0, rootPos2);

    MiningOptimizationTraining::ReturnPathNode returnPathNode1{
            childPos,
            MiningOptimizationTraining::NodeType::Test,
            {},
            {{arrivalData1, 1}, {arrivalData2, 1}, {arrivalData3, 1}, {arrivalData4, 1}}};
    returnPathNode1.nextPositions.emplace_back(
            MiningOptimizationTraining::ReturnPathNode{childPos, MiningOptimizationTraining::NodeType::Test}, 1);

    MiningOptimizationTraining::ReturnPath returnPath1{rootPos1};
    returnPath1.nextPositions.emplace_back(std::move(returnPathNode1), 1);

    MiningOptimizationTraining::ReturnPathNode returnPathNode2{
            childPos,
            MiningOptimizationTraining::NodeType::Test,
            {},
            {{arrivalData5, 1}, {arrivalData6, 1}, {arrivalData7, 1}, {arrivalData8, 1}}};
    returnPathNode2.nextPositions.emplace_back(
            MiningOptimizationTraining::ReturnPathNode{childPos, MiningOptimizationTraining::NodeType::Test}, 1);

    MiningOptimizationTraining::ReturnPath returnPath2{rootPos2};
    returnPath2.nextPositions.emplace_back(std::move(returnPathNode2), 1);

    std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::ReturnPath> rootNodes;
    rootNodes.emplace(rootPos1, std::move(returnPath1));
    rootNodes.emplace(rootPos2, std::move(returnPath2));

    trainingData.resourceToReturnPaths.emplace(TilePosition(0, 0), std::move(rootNodes));

    MiningOptimizationTraining::DataTransformer::transform(trainingData);

    MiningOptimization::MapData outputData;
    MiningOptimization::Serialization::setGameParameters("test");
    MiningOptimization::Serialization::readMapData(outputData);

    // Minimum next path length should be 45
    EXPECT_EQ(45, outputData.minimumNextPathLength);

    MiningOptimization::PositionAndVelocity expectedRootPos1(100, 100, 10, 10, 10, false);
    MiningOptimization::PositionAndVelocity expectedRootPos2(100, 101, 10, 10, 10, false);

    auto &outputPatchData = outputData.resourceToSerializedReturnPaths[TilePosition(0, 0)];
    auto validateRoot = [&](
            MiningOptimization::PositionAndVelocity rootPos,
            const std::vector<MiningOptimizationTraining::ReturnArrivalData> &expectedArrivalData,
            unsigned int expectedNextPathLength)
    {
        EXPECT_TRUE(outputPatchData.contains(rootPos));
        auto nextPositions = outputPatchData[rootPos].get().nextPositions;
        EXPECT_EQ(1, nextPositions.size());
        auto &nextPosition = nextPositions[0].first;
        EXPECT_EQ(expectedArrivalData.size(), nextPosition.arrivalDataAfterResend.size());

        for (const auto &expectedArrival : expectedArrivalData)
        {
            bool found = false;
            for (const auto &[actualArrival, _] : nextPosition.arrivalDataAfterResend)
            {
                if (actualArrival.arrivalDelay() == expectedArrival.arrivalDelay()
                    && (uint8_t)actualArrival.exitSpeed() == (uint8_t)expectedArrival.exitSpeed()
#if USE_NEXT_PATH_LENGTHS
                    && actualArrival.nextPathLength(outputData.minimumNextPathLength) == expectedNextPathLength
#endif
                    )
                {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found);
        }
    };

    validateRoot(expectedRootPos1, {arrivalData1, arrivalData2, arrivalData3, arrivalData4}, 45);
    validateRoot(expectedRootPos2, {arrivalData5, arrivalData6, arrivalData7, arrivalData8}, 75);
}

TEST(DataTransformerTests, DeserializesCompactly)
{
    MiningOptimization::MapData mapData;
    MiningOptimization::Serialization::setGameParameters(Maps::GetOne("Vermeer")->openbwHash);
    MiningOptimization::Serialization::readMapData(mapData);

    auto checkPathData = []<typename ObservationType>(
            const std::unordered_map<TilePosition, std::unordered_map<MiningOptimization::PositionAndVelocity,
                    MiningOptimization::SerializedPath<ObservationType>>> &pathData)
    {
        std::function<void(const std::vector<std::pair<MiningOptimization::PathNode<ObservationType>, uint8_t>> &)> checkNextNodes;

        checkNextNodes = [&checkNextNodes](const std::vector<std::pair<MiningOptimization::PathNode<ObservationType>, uint8_t>> &nextNodes)
        {
            EXPECT_EQ(nextNodes.capacity(), nextNodes.size());
            for (const auto &[node, _] : nextNodes)
            {
                EXPECT_EQ(node.arrivalDataAfterResend.capacity(), node.arrivalDataAfterResend.size());

                checkNextNodes(node.nextPositions);
                checkNextNodes(node.nextPositionsAfterResend);
            }
        };

        EXPECT_FALSE(pathData.empty());
        for (const auto &[_, rootNodes] : pathData)
        {
            EXPECT_FALSE(rootNodes.empty());
            for (const auto &[_, rootNode] : rootNodes)
            {
                checkNextNodes(rootNode.get().nextPositions);
            }
        }
    };
    checkPathData(mapData.resourceToSerializedGatherPaths);
    checkPathData(mapData.resourceToSerializedReturnPaths);
    EXPECT_EQ(mapData.positionDeltas.capacity(), mapData.positionDeltas.size());
}

TEST(DataTransformerTests, OccurrenceVectorsAreSorted)
{
    MiningOptimization::MapData mapData;
    MiningOptimization::Serialization::setGameParameters(Maps::GetOne("Vermeer")->openbwHash);
    MiningOptimization::Serialization::readMapData(mapData);

    auto checkPathData = []<typename ObservationType>(
            const std::unordered_map<TilePosition, std::unordered_map<MiningOptimization::PositionAndVelocity,
                    MiningOptimization::SerializedPath<ObservationType>>> &pathData)
    {
        auto checkOccurrences = [](auto &observations)
        {
            uint8_t last = OCCURRENCE_SCALE;
            for (const auto &[_, occurrences] : observations)
            {
                EXPECT_GE(last, occurrences);
                last = occurrences;
            }
        };

        std::function<void(const std::vector<std::pair<MiningOptimization::PathNode<ObservationType>, uint8_t>> &)> checkNextNodes;

        checkNextNodes = [&](const std::vector<std::pair<MiningOptimization::PathNode<ObservationType>, uint8_t>> &nextNodes)
        {
            for (const auto &[node, _] : nextNodes)
            {
                checkOccurrences(node.arrivalDataAfterResend);
                checkOccurrences(node.nextPositions);
                checkOccurrences(node.nextPositionsAfterResend);

                checkNextNodes(node.nextPositions);
                checkNextNodes(node.nextPositionsAfterResend);
            }
        };

        EXPECT_FALSE(pathData.empty());
        for (const auto &[_, rootNodes] : pathData)
        {
            EXPECT_FALSE(rootNodes.empty());
            for (const auto &[_, rootNode] : rootNodes)
            {
                checkNextNodes(rootNode.get().nextPositions);
            }
        }
    };
    checkPathData(mapData.resourceToSerializedGatherPaths);
    checkPathData(mapData.resourceToSerializedReturnPaths);
}
