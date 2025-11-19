#include "gtest/gtest.h"

#include "DataTransformer.h"
#include "MiningOptimizationV2/DataModel/Serialization.h"
//
//namespace
//{
//    MiningOptimizationTraining::PositionAndVelocity generateTestPosition(uint16_t value)
//    {
//        return {value, value, (int8_t)value, (int32_t)value, (int32_t)value};
//    }
//
//    MiningOptimizationTraining::GatherPathNode generateGatherPathNode(MiningOptimizationTraining::PositionAndVelocity pos)
//    {
//        MiningOptimizationTraining::GatherPathNode result{pos};
//        result.type = (MiningOptimizationTraining::NodeType)(pos.x % 6);
//        result.arrivalData.emplace(MiningOptimizationTraining::GatherArrivalData(pos.x), pos.x);
//        result.arrivalDataAfterResend.emplace(MiningOptimizationTraining::GatherArrivalData(pos.x - 5), pos.x - 5);
//        return result;
//    }
//
//    MiningOptimizationTraining::GatherPath generateGatherPath(MiningOptimizationTraining::PositionAndVelocity &pos,
//                                                              MiningOptimizationTraining::GatherPathNode &nextNode)
//    {
//        return MiningOptimizationTraining::GatherPath{
//            pos,
//            {{nextNode, nextNode.pos.x}}, // next positions
//            pos.x, // times explored
//            {{pos.x + 5, pos.x + 5}}, // no resend delay and occurrences
//            {{pos.x, pos.x}} // best delay and occurrences
//        };
//    }
//}

TEST(DataTransformerTests, PositionDeltaIndex)
{
    MiningOptimizationTraining::MapData trainingData;
    trainingData.mapHash = "test";

    MiningOptimizationTraining::PositionAndVelocity rootPos(100, 100, 10, 10, 10);
    MiningOptimizationTraining::PositionAndVelocity childPos1(105, 100, 15, 15, -15);
    MiningOptimizationTraining::PositionAndVelocity childPos2(105, 100, 20, -20, 20);
    MiningOptimizationTraining::PositionAndVelocity childPos3(95, 100, 10, 10, 10);

    trainingData.resourceToGatherPaths.emplace(
            TilePosition(0, 0),
            std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::GatherPath>{
                    {rootPos, MiningOptimizationTraining::GatherPath{
                            rootPos,
                            {{MiningOptimizationTraining::GatherPathNode{childPos1}, 1},
                             {MiningOptimizationTraining::GatherPathNode{childPos2}, 1},
                             {MiningOptimizationTraining::GatherPathNode{childPos3}, 1}}
                    }}
            });

    MiningOptimizationTraining::DataTransformer::transform(trainingData);

    MiningOptimizationV2::MapData outputData;
    MiningOptimizationV2::Serialization::setGameParameters("test");
    MiningOptimizationV2::Serialization::readMapData(outputData);

    // Ensure the position deltas contain the expected values
    EXPECT_EQ(2, outputData.positionDeltas.size());
    uint8_t pos1And2Index = 255;
    uint8_t pos3Index = 255;
    for (uint8_t i = 0; i < (uint8_t)outputData.positionDeltas.size(); i++)
    {
        if (outputData.positionDeltas[i] == std::make_pair((int8_t)5, (int8_t)0)) pos1And2Index = i;
        if (outputData.positionDeltas[i] == std::make_pair((int8_t)-5, (int8_t)0)) pos3Index = i;
    }
    EXPECT_LT(pos1And2Index, 2);
    EXPECT_LT(pos3Index, 2);

    // Ensure the root node in the output data has the expected values
    MiningOptimizationV2::PositionAndVelocity expectedRootPos(100, 100, 10, 10, 10);
    MiningOptimizationV2::PositionDeltaAndVelocity expectedChildPos1((pos1And2Index << 1) + 1, 15, 15, -15);
    MiningOptimizationV2::PositionDeltaAndVelocity expectedChildPos2((pos1And2Index << 1) + 1, 20, -20, 20);
    MiningOptimizationV2::PositionDeltaAndVelocity expectedChildPos3((pos3Index << 1), 17, 58, -98); // heading and velocity don't matter here

    auto &outputPatchData = outputData.resourceToGatherPaths[TilePosition(0, 0)];
    EXPECT_TRUE(outputPatchData.contains(expectedRootPos));
    EXPECT_EQ(expectedRootPos, outputPatchData[expectedRootPos].pos);
    EXPECT_EQ(3, outputPatchData[expectedRootPos].nextPositions.size());
    EXPECT_EQ(expectedChildPos1, outputPatchData[expectedRootPos].nextPositions[0].first.pos);
    EXPECT_EQ(expectedChildPos2, outputPatchData[expectedRootPos].nextPositions[1].first.pos);
    EXPECT_EQ(expectedChildPos3, outputPatchData[expectedRootPos].nextPositions[2].first.pos);
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

    trainingData.resourceToGatherPaths.emplace(
            TilePosition(0, 0),
            std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::GatherPath>{
                    {rootPos, MiningOptimizationTraining::GatherPath{
                            rootPos,
                            {{MiningOptimizationTraining::GatherPathNode{childPos1}, 1},
                             {MiningOptimizationTraining::GatherPathNode{childPos2}, 1},
                             {MiningOptimizationTraining::GatherPathNode{childPos3}, 1},
                             {MiningOptimizationTraining::GatherPathNode{childPos4}, 1}}
                    }}
            });
    trainingData.resourceToGatherPaths.emplace(
            TilePosition(1, 0),
            std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::GatherPath>{
                    {rootPos, MiningOptimizationTraining::GatherPath{
                            rootPos,
                            {{MiningOptimizationTraining::GatherPathNode{childPos1}, 1},
                             {MiningOptimizationTraining::GatherPathNode{childPos2}, 1},
                             {MiningOptimizationTraining::GatherPathNode{childPos3}, 1},
                             {MiningOptimizationTraining::GatherPathNode{childPos4}, 1},
                             {MiningOptimizationTraining::GatherPathNode{childPos5}, 1},
                             {MiningOptimizationTraining::GatherPathNode{childPos6}, 1},
                             {MiningOptimizationTraining::GatherPathNode{childPos7}, 1}}
                    }}
            });

    MiningOptimizationTraining::DataTransformer::transform(trainingData);

    MiningOptimizationV2::MapData outputData;
    MiningOptimizationV2::Serialization::setGameParameters("test");
    MiningOptimizationV2::Serialization::readMapData(outputData);

    MiningOptimizationV2::PositionAndVelocity expectedRootPos(100, 100, 10, 10, 10);

    auto expectSum = [&](
            const std::unordered_map<MiningOptimizationV2::PositionAndVelocity, MiningOptimizationV2::GatherPath> &data)
    {
        EXPECT_TRUE(data.contains(expectedRootPos));
        unsigned int total = 0;
        for (const auto &[_, occurrenceRate] : data.at(expectedRootPos).nextPositions)
        {
            total += (unsigned int)occurrenceRate;
        }
        EXPECT_EQ(255, total);
    };

    expectSum(outputData.resourceToGatherPaths[TilePosition(0, 0)]);
    expectSum(outputData.resourceToGatherPaths[TilePosition(1, 0)]);
}
