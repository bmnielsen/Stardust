#include "gtest/gtest.h"

#include "Serialization.h"

namespace
{
    MiningOptimizationTraining::PositionAndVelocity generateTestPosition(uint16_t value)
    {
        return {value, value, (int8_t)value, (int32_t)value, (int32_t)value};
    }

    MiningOptimizationTraining::GatherPathNode generateGatherPathNode(MiningOptimizationTraining::PositionAndVelocity pos)
    {
        MiningOptimizationTraining::GatherPathNode result;
        result.positionDifferenceFromPreviousNode = { pos.x, pos.y };
        result.type = (MiningOptimizationTraining::NodeType)(pos.x % 6);
        result.timesExplored = pos.x;
        result.arrivalData.emplace(MiningOptimizationTraining::GatherArrivalData(pos.x), pos.x);
        result.arrivalDataAfterResend.emplace(MiningOptimizationTraining::GatherArrivalData(pos.x - 5), pos.x - 5);
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

    void assertGatherPathNodesVectorEqual(std::vector<std::pair<MiningOptimizationTraining::GatherPathNode, uint32_t>> &expected,
                                          std::vector<std::pair<MiningOptimizationTraining::GatherPathNode, uint32_t>> &actual);

    void assertGatherPathNodesEqual(MiningOptimizationTraining::GatherPathNode &expected,
                                    MiningOptimizationTraining::GatherPathNode &actual)
    {
        ASSERT_EQ(expected.positionDifferenceFromPreviousNode, actual.positionDifferenceFromPreviousNode);
        ASSERT_EQ(expected.type, actual.type);

        auto occurrenceCountEqual = [](uint32_t &expected, uint32_t &actual)
        {
            ASSERT_EQ(expected, actual);
        };

        assertMapsEqual(expected.arrivalData, actual.arrivalData, occurrenceCountEqual);
        assertMapsEqual(expected.arrivalDataAfterResend, actual.arrivalDataAfterResend, occurrenceCountEqual);

        assertGatherPathNodesVectorEqual(expected.nextPositions, actual.nextPositions);
        assertGatherPathNodesVectorEqual(expected.nextPositionsAfterResend, actual.nextPositionsAfterResend);
    }

    void assertGatherPathNodesVectorEqual(std::vector<std::pair<MiningOptimizationTraining::GatherPathNode, uint32_t>> &expected,
                                          std::vector<std::pair<MiningOptimizationTraining::GatherPathNode, uint32_t>> &actual)
    {
        ASSERT_EQ(expected.size(), actual.size());
        for (int i = 0; i < expected.size(); i++)
        {
            assertGatherPathNodesEqual(expected[i].first, actual[i].first);
            ASSERT_EQ(expected[i].second, actual[i].second);
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
    gatherObservations[0].nextPositions.emplace_back(gatherObservations[1], gatherObservations[1].positionDifferenceFromPreviousNode.x);
    gatherObservations[0].nextPositions.emplace_back(gatherObservations[2], gatherObservations[2].positionDifferenceFromPreviousNode.x);
    gatherObservations[0].nextPositionsAfterResend.emplace_back(gatherObservations[3], gatherObservations[3].positionDifferenceFromPreviousNode.x);
    gatherObservations[0].nextPositionsAfterResend.emplace_back(gatherObservations[4], gatherObservations[4].positionDifferenceFromPreviousNode.x);
    gatherObservations[5].nextPositions.emplace_back(gatherObservations[6], gatherObservations[6].positionDifferenceFromPreviousNode.x);
    gatherObservations[5].nextPositionsAfterResend.emplace_back(gatherObservations[7], gatherObservations[7].positionDifferenceFromPreviousNode.x);

    // Create the expected map data
    MiningOptimizationTraining::MapData expected;
    expected.resourceToGatherPaths.emplace(
            TilePosition(0, 0),
            std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::GatherPath>{
                {pos[0], MiningOptimizationTraining::GatherPath{pos[0], {
                    {gatherObservations[0], gatherObservations[0].positionDifferenceFromPreviousNode.x}
                }}},
                {pos[8], MiningOptimizationTraining::GatherPath{pos[8], {
                    {gatherObservations[8], gatherObservations[8].positionDifferenceFromPreviousNode.x}
                }}}
            });
    expected.resourceToGatherPaths.emplace(
            TilePosition(1, 1),
            std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::GatherPath>{
                {pos[5], MiningOptimizationTraining::GatherPath{pos[5], {
                    {gatherObservations[5], gatherObservations[5].positionDifferenceFromPreviousNode.x}
                }}}
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
