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
        MiningOptimizationTraining::GatherPathNode result{pos};
        result.type = (MiningOptimizationTraining::NodeType)(pos.x % 6);
        result.arrivalData.emplace(MiningOptimizationTraining::GatherArrivalData(pos.x), pos.x);
        result.arrivalDataAfterResend.emplace(MiningOptimizationTraining::GatherArrivalData(pos.x - 5), pos.x - 5);
        return result;
    }

    MiningOptimizationTraining::InitialWorkerGatherPathNode generateInitialWorkerGatherPathNode(MiningOptimizationTraining::PositionAndVelocity pos)
    {
        MiningOptimizationTraining::InitialWorkerGatherPathNode result{
            BWAPI::ExactPosition{pos.x, pos.y, pos.heading, pos.velocityX, pos.velocityY}
        };
        result.type = (MiningOptimizationTraining::NodeType)(pos.x % 6);
        result.arrivalDataActionAtArrival.arrivalDelay = pos.x;
        result.arrivalDataActionAtArrival.facingPatch = (pos.x % 2 == 0);
        result.arrivalDataActionAtArrival.nextPathStartPosition =
                BWAPI::ExactPosition{pos.x + 2U, pos.y + 2U, pos.heading, pos.velocityX, pos.velocityY};
        result.arrivalDataActionAfterArrival.arrivalDelay = pos.x + 1;
        result.arrivalDataActionAfterArrival.facingPatch = (pos.x % 2 != 0);
        result.arrivalDataActionAfterArrival.nextPathStartPosition =
                BWAPI::ExactPosition{pos.x + 3U, pos.y + 3U, pos.heading, pos.velocityX, pos.velocityY};
        if (pos.x % 2 != 0)
        {
            result.arrivalDataAfterResendActionAtArrival = MiningOptimizationTraining::InitialWorkerGatherArrivalData{
                pos.x,
                (pos.x % 4 == 0),
                BWAPI::ExactPosition{pos.x + 3U, pos.y + 3U, pos.heading, pos.velocityX, pos.velocityY}
            };
            result.arrivalDataAfterResendActionAfterArrival = MiningOptimizationTraining::InitialWorkerGatherArrivalData{
                pos.x,
                (pos.x % 4 != 0),
                BWAPI::ExactPosition{pos.x + 4U, pos.y + 4U, pos.heading, pos.velocityX, pos.velocityY}
            };
        }
        return result;
    }

    MiningOptimizationTraining::GatherPath generateGatherPath(MiningOptimizationTraining::PositionAndVelocity &pos,
                                                              MiningOptimizationTraining::GatherPathNode nextNode)
    {
        std::vector<std::pair<MiningOptimizationTraining::GatherPathNode, uint32_t>> nextPositions;
        nextPositions.emplace_back(std::move(nextNode), nextNode.pos.x);

        std::map<MiningOptimization::CannonPlacement, std::vector<std::pair<MiningOptimizationTraining::GatherPathNode, uint32_t>>> nextPositionsMap;
        nextPositionsMap.emplace(MiningOptimization::CannonPlacement{0}, std::move(nextPositions));

        return MiningOptimizationTraining::GatherPath{
            pos,
            std::move(nextPositionsMap),
            pos.x, pos.x, // times explored
            {{pos.x, pos.x}} // best delay and occurrences
        };
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
        ASSERT_EQ(expected.pos, actual.pos);
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

    void assertInitialWorkerGatherPathNodesEqual(MiningOptimizationTraining::InitialWorkerGatherPathNode &expected,
                                                 MiningOptimizationTraining::InitialWorkerGatherPathNode &actual)
    {
        ASSERT_EQ(expected.pos, actual.pos);
        ASSERT_EQ(expected.type, actual.type);
        ASSERT_EQ(expected.arrivalDataActionAtArrival, actual.arrivalDataActionAtArrival);
        ASSERT_EQ(expected.arrivalDataActionAfterArrival, actual.arrivalDataActionAfterArrival);
        ASSERT_EQ(expected.arrivalDataAfterResendActionAtArrival, actual.arrivalDataAfterResendActionAtArrival);
        ASSERT_EQ(expected.arrivalDataAfterResendActionAfterArrival, actual.arrivalDataAfterResendActionAfterArrival);
        ASSERT_EQ(expected.nextPosition != nullptr, actual.nextPosition != nullptr);
        if (expected.nextPosition && actual.nextPosition)
        {
            assertInitialWorkerGatherPathNodesEqual(*expected.nextPosition, *actual.nextPosition);
        }
        ASSERT_EQ(expected.nextPositionAfterResend != nullptr, actual.nextPositionAfterResend != nullptr);
        if (expected.nextPositionAfterResend && actual.nextPositionAfterResend)
        {
            assertInitialWorkerGatherPathNodesEqual(*expected.nextPositionAfterResend, *actual.nextPositionAfterResend);
        }
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
        assertGatherPathNodesVectorEqual(expected.nextPositions[{}], actual.nextPositions[{}]);
    }
}

// Tests that we can serialize some data and read it back
TEST(SerializationTests, WriteAndReadBack)
{
    Log::SetOutputToConsole(true);

    // Set up sample gather data
    MiningOptimizationTraining::PositionAndVelocity pos[9];
    std::vector<MiningOptimizationTraining::GatherPathNode> gatherObservations;
    for (int i = 0; i < 9; i++)
    {
        pos[i] = generateTestPosition(10 * (i+1));
        gatherObservations.push_back(generateGatherPathNode(pos[i]));
    }
    gatherObservations[0].nextPositions.emplace_back(std::move(gatherObservations[1]), gatherObservations[1].pos.x);
    gatherObservations[0].nextPositions.emplace_back(std::move(gatherObservations[2]), gatherObservations[2].pos.x);
    gatherObservations[0].nextPositionsAfterResend.emplace_back(std::move(gatherObservations[3]), gatherObservations[3].pos.x);
    gatherObservations[0].nextPositionsAfterResend.emplace_back(std::move(gatherObservations[4]), gatherObservations[4].pos.x);
    gatherObservations[5].nextPositions.emplace_back(std::move(gatherObservations[6]), gatherObservations[6].pos.x);
    gatherObservations[5].nextPositionsAfterResend.emplace_back(std::move(gatherObservations[7]), gatherObservations[7].pos.x);

    std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::GatherPath> rootNodes1;
    rootNodes1.emplace(pos[0], generateGatherPath(pos[0], std::move(gatherObservations[0])));
    rootNodes1.emplace(pos[8], generateGatherPath(pos[8], std::move(gatherObservations[8])));
    std::unordered_map<MiningOptimizationTraining::PositionAndVelocity, MiningOptimizationTraining::GatherPath> rootNodes2;
    rootNodes2.emplace(pos[5], generateGatherPath(pos[5], std::move(gatherObservations[5])));

    // Create the expected map data
    MiningOptimizationTraining::MapData expected;
    expected.resourceToGatherPaths.emplace(TilePosition(0, 0), std::move(rootNodes1));
    expected.resourceToGatherPaths.emplace(TilePosition(1, 1), std::move(rootNodes2));

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

// Tests that we can serialize some initial worker data and read it back
TEST(SerializationTests, InitialWorkersWriteAndReadBack)
{
    Log::SetOutputToConsole(true);

    // TODO: Add other fields

    // Set up sample gather data
    MiningOptimizationTraining::PositionAndVelocity pos[9];
    std::vector<MiningOptimizationTraining::InitialWorkerGatherPathNode> gatherObservations;
    for (int i = 0; i < 9; i++)
    {
        pos[i] = generateTestPosition(10 * (i+1));
        gatherObservations.push_back(generateInitialWorkerGatherPathNode(pos[i]));
    }
    gatherObservations[1].nextPosition =
            std::make_unique<MiningOptimizationTraining::InitialWorkerGatherPathNode>(std::move(gatherObservations[3]));
    gatherObservations[0].nextPosition =
            std::make_unique<MiningOptimizationTraining::InitialWorkerGatherPathNode>(std::move(gatherObservations[1]));
    gatherObservations[0].nextPositionAfterResend =
            std::make_unique<MiningOptimizationTraining::InitialWorkerGatherPathNode>(std::move(gatherObservations[2]));
    gatherObservations[5].nextPosition =
            std::make_unique<MiningOptimizationTraining::InitialWorkerGatherPathNode>(std::move(gatherObservations[7]));
    gatherObservations[4].nextPosition =
            std::make_unique<MiningOptimizationTraining::InitialWorkerGatherPathNode>(std::move(gatherObservations[5]));
    gatherObservations[4].nextPositionAfterResend =
            std::make_unique<MiningOptimizationTraining::InitialWorkerGatherPathNode>(std::move(gatherObservations[6]));

    std::map<TilePosition, MiningOptimizationTraining::InitialWorkerGatherPathNode> rootNodes1;
    rootNodes1.emplace(TilePosition{1,1}, std::move(gatherObservations[0]));
    rootNodes1.emplace(TilePosition{1,2}, std::move(gatherObservations[8]));
    std::map<TilePosition, MiningOptimizationTraining::InitialWorkerGatherPathNode> rootNodes2;
    rootNodes2.emplace(TilePosition{1,1}, std::move(gatherObservations[4]));

    // Create the expected map data
    MiningOptimizationTraining::InitialWorkerMapData expected;
    expected.startingWorkerPositionToPatchToFirstGatherPath.emplace(BWAPI::ExactPosition(1,1,0,0,0), std::move(rootNodes1));
    expected.startingWorkerPositionToPatchToFirstGatherPath.emplace(BWAPI::ExactPosition(1,2,0,0,0), std::move(rootNodes2));

    // Serialize the data
    MiningOptimizationTraining::Serialization::setGameParameters("test");
    MiningOptimizationTraining::Serialization::writeMapData(expected);

    // Deserialize the data to a new structure
    MiningOptimizationTraining::InitialWorkerMapData actual;
    MiningOptimizationTraining::Serialization::readMapData(actual);

    // Assert
    assertMapsEqual(expected.startingWorkerPositionToPatchToFirstGatherPath,
                    actual.startingWorkerPositionToPatchToFirstGatherPath,
                    std::function{[](
                            std::map<TilePosition, MiningOptimizationTraining::InitialWorkerGatherPathNode> &expected,
                            std::map<TilePosition, MiningOptimizationTraining::InitialWorkerGatherPathNode> &actual)
                            {
                                assertMapsEqual(expected, actual, std::function{&assertInitialWorkerGatherPathNodesEqual});
                            }});
}
