#include "gtest/gtest.h"

#include "Serialization.h"

namespace
{
    MiningOptimizationTraining::PositionAndVelocity generateTestPosition(uint16_t value)
    {
        return {value, value, (int8_t)value, (int8_t)value, (int8_t)value};
    }

    MiningOptimizationTraining::GatherPathNode generateGatherPathNode(MiningOptimizationTraining::PositionAndVelocity pos)
    {
        MiningOptimizationTraining::GatherPathNode result;
        result.positionDifferenceFromPreviousNode = { pos.x, pos.y };
        result.type = (MiningOptimizationTraining::NodeType)(pos.x % 6);
        result.timesExplored = pos.x;
        result.arrivalData.insert(MiningOptimizationTraining::GatherArrivalData(pos.x));
        result.arrivalDataAfterResend.insert(MiningOptimizationTraining::GatherArrivalData(pos.x - 5));
        return result;
    }

    template <typename M>
    void assertMapsEqual(M &expected, M &actual, const auto& valueComparator)
    {
        ASSERT_EQ(expected.size(), actual.size());
        for (auto &[expectedKey, expectedValue] : expected)
        {
            ASSERT_TRUE(actual.contains(expectedKey));
            valueComparator(expectedValue, actual[expectedKey]);
        }
    }

    template <typename M>
    void assertSetsEqual(M &expected, M &actual)
    {
        ASSERT_EQ(expected.size(), actual.size());
        for (auto &expectedKey : expected)
        {
            ASSERT_TRUE(actual.contains(expectedKey));
        }
    }

    void assertGatherPathNodesVectorEqual(std::vector<MiningOptimizationTraining::GatherPathNode> &expected,
                                          std::vector<MiningOptimizationTraining::GatherPathNode> &actual);

    void assertGatherPathNodesEqual(MiningOptimizationTraining::GatherPathNode &expected,
                                    MiningOptimizationTraining::GatherPathNode &actual)
    {
        ASSERT_EQ(expected.positionDifferenceFromPreviousNode, actual.positionDifferenceFromPreviousNode);
        ASSERT_EQ(expected.type, actual.type);
        assertSetsEqual(expected.arrivalData, actual.arrivalData);
        assertSetsEqual(expected.arrivalDataAfterResend, actual.arrivalDataAfterResend);

        assertGatherPathNodesVectorEqual(expected.nextPositions, actual.nextPositions);
        assertGatherPathNodesVectorEqual(expected.nextPositionsAfterResend, actual.nextPositionsAfterResend);
    }

    void assertGatherPathNodesVectorEqual(std::vector<MiningOptimizationTraining::GatherPathNode> &expected,
                                          std::vector<MiningOptimizationTraining::GatherPathNode> &actual)
    {
        ASSERT_EQ(expected.size(), actual.size());
        for (int i = 0; i < expected.size(); i++)
        {
            assertGatherPathNodesEqual(expected[i], actual[i]);
        }
    }

    void assertGatherPathsEqual(MiningOptimizationTraining::GatherPath &expected,
                                MiningOptimizationTraining::GatherPath &actual)
    {
        ASSERT_EQ(expected.pos, actual.pos);
        assertGatherPathNodesVectorEqual(expected.nextPositions, actual.nextPositions);
    }
}

// Tests that we can serialize some data and read it back
TEST(SerializationTests, WriteAndReadBack)
{
    Log::SetOutputToConsole(true);

    // Set up sample gather data
    MiningOptimizationTraining::PositionAndVelocity pos[9];
    MiningOptimizationTraining::GatherPathNode gatherObservations[9];
    for (int i = 0; i < 9; i++)
    {
        pos[i] = generateTestPosition(10 * (i+1));
        gatherObservations[i] = generateGatherPathNode(pos[i]);
    }
    gatherObservations[0].nextPositions.push_back(gatherObservations[1]);
    gatherObservations[0].nextPositions.push_back(gatherObservations[2]);
    gatherObservations[0].nextPositionsAfterResend.push_back(gatherObservations[3]);
    gatherObservations[0].nextPositionsAfterResend.push_back(gatherObservations[4]);
    gatherObservations[5].nextPositions.push_back(gatherObservations[6]);
    gatherObservations[5].nextPositionsAfterResend.push_back(gatherObservations[7]);

    // Create the expected map data
    MiningOptimizationTraining::MapData expected;
    expected.resourceToGatherPaths.emplace(
            TilePosition(0, 0),
            std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::GatherPath>{
                {pos[0], MiningOptimizationTraining::GatherPath{pos[0], {gatherObservations[0]}}},
                {pos[8], MiningOptimizationTraining::GatherPath{pos[8], {gatherObservations[8]}}}
            });
    expected.resourceToGatherPaths.emplace(
            TilePosition(1, 1),
            std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::GatherPath>{
                {pos[5], MiningOptimizationTraining::GatherPath{pos[5], {gatherObservations[5]}}}
            });

    // Serialize the data
    MiningOptimizationTraining::Serialization::setGameParameters("test");
    MiningOptimizationTraining::Serialization::writeMapData(expected);

    // Deserialize the data to a new structure
    MiningOptimizationTraining::MapData actual;
    MiningOptimizationTraining::Serialization::readMapData(actual);

    // Assert
    assertMapsEqual(expected.resourceToGatherPaths,
                    actual.resourceToGatherPaths,
                    std::function{[](
                            std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::GatherPath> &expected,
                            std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::GatherPath> &actual)
                            {
                                assertMapsEqual(expected, actual, std::function{&assertGatherPathsEqual});
                            }});
}
