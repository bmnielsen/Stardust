#include "BWTest.h"

#include "MiningOptimizationTraining/PathExploration/ExploreStartPositionsModule.h"
#include "ClearOpponentUnitsModule.h"

using namespace MiningOptimizationTraining;

namespace
{
    void run(BWTest &test, ExploreStartPositionsModuleOptions options)
    {
        options.loadInitialWorkerMapData = false;

        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new ClearOpponentUnitsModule();
        };
        test.myModule = [&]()
        {
            return new ExploreStartPositionsModule<ExploreStartPosition>(options);
        };
        test.allowOpponentOutput = false;
        test.expectWin = false;
        test.randomSeed = 42;
        test.writeReplay = false;
        test.frameLimit = 100;
        test.timeLimit = INT_MAX;
        test.run();
    }

    void addResendAlwaysArrives(const std::string &mapHash)
    {
        MapData data;
        Serialization::setGameParameters(mapHash);
        std::cout << "Loading path data for " << mapHash << "..." << std::endl;
        Serialization::readMapData(data);

        std::cout << "Processing paths..." << std::endl;

        std::function<void(std::vector<std::pair<PathNode<GatherArrivalData>, uint32_t>> &, unsigned int, unsigned int, unsigned int)> processNextNodes;
        processNextNodes = [&processNextNodes](
            std::vector<std::pair<PathNode<GatherArrivalData>, uint32_t>> &nextNodes,
            unsigned int successfulDelta,
            unsigned int countOnNoResendPath,
            unsigned int countSinceHadResendData)
        {
            for (auto &[node, _] : nextNodes)
            {
                unsigned int nodeSuccessfulDelta = successfulDelta;
                unsigned int nodeCountSinceHadResendData = countSinceHadResendData;
                if (node.arrivalDataAfterResend.empty())
                {
                    ++nodeSuccessfulDelta;
                    ++nodeCountSinceHadResendData;
                }
                else
                {
                    nodeCountSinceHadResendData = 0;
                    unsigned int maxArrivalDelay = 0;
                    for (const auto &[resendArrivalData, _] : node.arrivalDataAfterResend)
                    {
                        maxArrivalDelay = std::max(maxArrivalDelay, resendArrivalData.arrivalDelay());
                    }
                    if (maxArrivalDelay > 11)
                    {
                        nodeSuccessfulDelta = 0;
                    }
                    else
                    {
                        ++nodeSuccessfulDelta;
                    }
                }

                if (node.nextPositions.empty())
                {
                    // If there have been many nodes since having resend data, it is a stable path and therefore match normal resend timing
                    if (countSinceHadResendData > 11)
                    {
                        nodeSuccessfulDelta = 11;
                    }

                    for (auto &[savedArrivalData, _] : node.arrivalData)
                    {
                        if (nodeSuccessfulDelta == countOnNoResendPath)
                        {
                            savedArrivalData.resendAlwaysArrivesDelta = (UINT8_MAX - 1);
                        }
                        else
                        {
                            savedArrivalData.resendAlwaysArrivesDelta = nodeSuccessfulDelta;
                        }
                    }
                }

                processNextNodes(node.nextPositions, nodeSuccessfulDelta, countOnNoResendPath + 1, nodeCountSinceHadResendData);
                processNextNodes(node.nextPositionsAfterResend, 0, 1, 0);
            }
        };

        for (auto &[_, rootNodes] : data.resourceToGatherPaths)
        {
            for (auto &[pos, rootNode] : rootNodes)
            {
                for (auto &[_, nodes] : rootNode.nextPositions)
                {
                    processNextNodes(nodes, 0, 1, 0);
                }
            }
        }

        std::cout << "Saving path data for " << mapHash << "..." << std::endl;
        Serialization::writeMapData(data);

        std::cout << "Done!" << std::endl;
    }
}

TEST(PathExploration, VermeerSingleWorker)
{
    ExploreStartPositionsModuleOptions options;
    options.onePatch = BWAPI::TilePosition(2, 11);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    run(test, options);
}

TEST(PathExploration, VermeerOneBase)
{
    ExploreStartPositionsModuleOptions options;
    options.oneBase = BWAPI::TilePosition(7, 6);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    run(test, options);
}

TEST(PathExploration, Vermeer)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    run(test, {});
}

TEST(PathExploration, BenzeneOneBase)
{
    ExploreStartPositionsModuleOptions options;
    options.oneBase = BWAPI::TilePosition(7, 96);

    BWTest test;
    test.map = Maps::GetOne("Benzene");
    run(test, options);
}

TEST(PathExploration, Benzene)
{
    BWTest test;
    test.map = Maps::GetOne("Benzene");
    run(test, {});
}

TEST(PathExploration, AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        run(test, {});
    });
}

TEST(AddResendAlwaysArrivesToArrivalNodes, Benzene)
{
    BWTest test;
    test.map = Maps::GetOne("Benzene");
    addResendAlwaysArrives(test.map->openbwHash);
}

TEST(AddResendAlwaysArrivesToArrivalNodes, AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        addResendAlwaysArrives(test.map->openbwHash);
    });
}
