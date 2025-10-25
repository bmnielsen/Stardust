#include "gtest/gtest.h"

#include "Serialization.h"

namespace
{
    MiningOptimizationTraining::PositionOnPath generateTestPosition(int value)
    {
        MiningOptimizationTraining::PositionOnPath result;
        result.x = result.y = result.dXSubpixel = result.dYSubpixel = result.heading = value;
#if USE_VELOCITY
        result.velocityX = result.velocityY = value;
#endif
        return result;
    }

    MiningOptimizationTraining::GatherObservations generateGatherObservations(MiningOptimizationTraining::PositionOnPath pos)
    {
        MiningOptimizationTraining::GatherObservations result;
        result.pos = pos;
        result.occurrences = pos.x;
        result.arrivalObservations.arrivalToOccurrences.emplace(MiningOptimizationTraining::ArrivalData(pos.x), pos.x);
        result.arrivalObservations.collisions = pos.x;
        result.arrivalObservations.nonCollisions = pos.x;
        result.arrivalObservationsAfterResend.arrivalToOccurrences.emplace(MiningOptimizationTraining::ArrivalData(pos.x - 5), pos.x - 5);
        result.arrivalObservationsAfterResend.collisions = pos.x - 5;
        result.arrivalObservationsAfterResend.nonCollisions = pos.x - 5;
        return result;
    }

    template <typename M, typename V>
    void assertMapsEqual(M &expected, M &actual, std::function<void(V&, V&)> valueComparator)
    {
        ASSERT_EQ(expected.size(), actual.size());
        for (auto &[expectedKey, expectedValue] : expected)
        {
            ASSERT_TRUE(actual.contains(expectedKey));
            valueComparator(expectedValue, actual[expectedKey]);
        }
    }

    void assertGatherArrivalObservationsEqual(MiningOptimizationTraining::GatherArrivalObservations &expected,
                                              MiningOptimizationTraining::GatherArrivalObservations &actual)
    {
        assertMapsEqual(expected.arrivalToOccurrences,
                        actual.arrivalToOccurrences,
                        std::function{[](uint32_t &expected, uint32_t &actual){ ASSERT_EQ(expected, actual); }});
        ASSERT_EQ(expected.collisions, actual.collisions);
        ASSERT_EQ(expected.nonCollisions, actual.nonCollisions);
    }

    void assertGatherObservationsEqual(MiningOptimizationTraining::GatherObservations &expected,
                                       MiningOptimizationTraining::GatherObservations &actual)
    {
        ASSERT_EQ(expected.pos, actual.pos);
        ASSERT_EQ(expected.occurrences, actual.occurrences);
        assertGatherArrivalObservationsEqual(expected.arrivalObservations, actual.arrivalObservations);
        assertGatherArrivalObservationsEqual(expected.arrivalObservationsAfterResend, actual.arrivalObservationsAfterResend);

        auto assertGatherObservationsVectorEqual = [](std::vector<MiningOptimizationTraining::GatherObservations> &expected,
                                                      std::vector<MiningOptimizationTraining::GatherObservations> &actual)
        {
            ASSERT_EQ(expected.size(), actual.size());
            for (int i = 0; i < expected.size(); i++)
            {
                assertGatherObservationsEqual(expected[i], actual[i]);
            }
        };
        assertGatherObservationsVectorEqual(expected.nextPositions, actual.nextPositions);
        assertGatherObservationsVectorEqual(expected.nextPositionsAfterResend, actual.nextPositionsAfterResend);
    }
}

// Tests that we can serialize some data and read it back
TEST(SerializationTests, WriteAndReadBack)
{
    Log::SetOutputToConsole(true);

    // Set up sample gather data
    MiningOptimizationTraining::PositionOnPath pos[9];
    MiningOptimizationTraining::GatherObservations gatherObservations[9];
    for (int i = 0; i < 9; i++)
    {
        pos[i] = generateTestPosition(10 * (i+1));
        gatherObservations[i] = generateGatherObservations(pos[i]);
    }
    gatherObservations[0].nextPositions.push_back(gatherObservations[1]);
    gatherObservations[0].nextPositions.push_back(gatherObservations[2]);
    gatherObservations[0].nextPositionsAfterResend.push_back(gatherObservations[3]);
    gatherObservations[0].nextPositionsAfterResend.push_back(gatherObservations[4]);
    gatherObservations[5].nextPositions.push_back(gatherObservations[6]);
    gatherObservations[5].nextPositionsAfterResend.push_back(gatherObservations[7]);

    // Create the expected map data
    MiningOptimizationTraining::MapData expected;
    expected.resourceToGatherRootNodes.emplace(
            TilePosition(0, 0),
            std::unordered_map<MiningOptimizationTraining::PositionOnPath, MiningOptimizationTraining::GatherObservations>{
                {pos[0], gatherObservations[0]},
                {pos[8], gatherObservations[8]}
            });
    expected.resourceToGatherRootNodes.emplace(
            TilePosition(1, 1),
            std::unordered_map<MiningOptimizationTraining::PositionOnPath, MiningOptimizationTraining::GatherObservations>{
                {pos[5], gatherObservations[5]}
            });

    // Serialize the data
    MiningOptimizationTraining::Serialization::setGameParameters("test");
    MiningOptimizationTraining::Serialization::writeMapData(expected);

    // Deserialize the data to a new structure
    MiningOptimizationTraining::MapData actual;
    MiningOptimizationTraining::Serialization::readMapData(actual);

    // Assert
    assertMapsEqual(expected.resourceToGatherRootNodes,
                    actual.resourceToGatherRootNodes,
                    std::function{[](
                            std::unordered_map<MiningOptimizationTraining::PositionOnPath, MiningOptimizationTraining::GatherObservations> &expected,
                            std::unordered_map<MiningOptimizationTraining::PositionOnPath, MiningOptimizationTraining::GatherObservations> &actual)
                            {
                                assertMapsEqual(expected, actual, std::function{&assertGatherObservationsEqual});
                            }});
}
