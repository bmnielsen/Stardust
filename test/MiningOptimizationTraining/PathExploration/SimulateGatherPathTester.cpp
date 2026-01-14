#include "SimulateGatherPathTester.h"

#include "BWAPI/StateCopy.h"
#include "BWAPI/PrepareGatherPathOptions.h"
#include "BWAPI/PrepareGatherPathResult.h"
#include "BWAPI/SimulateGatherPathOptions.h"
#include "BWAPI/SimulateGatherPathResult.h"

#include "PathExplorationUtils.h"
#include "Geo.h"

#include <random>

namespace MiningOptimizationTraining
{
    namespace
    {
        std::default_random_engine rng(42); // NOLINT(*-msc51-cpp) - using fixed sequence for reproducability
        std::vector<std::set<int>> resendCombinations;
        std::vector<std::set<int>> resendCombinationsTwoResends;

        std::set<int> &chooseResendCombination(int arrivalFrame, int maxResends)
        {
            // Initialize resend combinations lazily
            if (resendCombinations.empty())
            {
                resendCombinations.emplace_back(); // the no resend option
                resendCombinationsTwoResends.emplace_back(); // the no resend option
                for (int firstResend = -50; firstResend <= -5; firstResend++)
                {
                    resendCombinations.push_back({firstResend});
                    resendCombinationsTwoResends.push_back({firstResend});
                    for (int secondResend = (firstResend + 1); secondResend <= -5; secondResend++)
                    {
                        if (secondResend == (firstResend + BWAPI::Broodwar->getLatencyFrames())) continue;
                        resendCombinationsTwoResends.push_back({firstResend, secondResend});
                    }
                }
            }

            std::vector<std::set<int>> &combinations = (maxResends == 2) ? resendCombinationsTwoResends : resendCombinations;

            while (true)
            {
                // Pick a combination
                std::uniform_int_distribution<size_t> dist(0, combinations.size() - 1);
                auto &chosenCombination = combinations[dist(rng)];

                // Validate if it is usable
                bool usable = true;
                for (auto &resendFrameDelta : chosenCombination)
                {
                    int resendFrame = arrivalFrame + resendFrameDelta;
                    if (resendFrame < (currentFrame + BWAPI::Broodwar->getLatencyFrames()))
                    {
                        usable = false;
                        break;
                    }
                }
                if (usable) return chosenCombination;
            }
        }
    }

    void SimulateGatherPathTester::update()
    {
        // For this testing, we don't care about the distinction between delivery at arrival or delivery not at arrival, so we just pass
        // the same paths for both cases

        auto setGatherArrivalData = [&](auto &simulatedPath)
        {
            expectedGatherArrivalData = std::make_unique<GatherArrivalData>(
                    GatherArrivalData::createFromSimulatedPaths(simulatedPath, simulatedPath, patch));
        };

        auto setReturnArrivalData = [&](auto &simulatedPath)
        {
            expectedReturnArrivalData = std::make_unique<ReturnArrivalData>(
                    ReturnArrivalData::createFromSimulatedPaths(simulatedPath, simulatedPath));
        };

        auto postValidateGatherPath =
                [&](auto &simulatedPathWithActionAtArrival,
                    auto &simulatedPathWithActionAfterArrival)
        {
            // For gather, the next start positions and squared speeds should be the same
            EXPECT_EQ(simulatedPathWithActionAtArrival.nextPathStartPosition, simulatedPathWithActionAfterArrival.nextPathStartPosition);
            EXPECT_EQ(simulatedPathWithActionAtArrival.squaredSpeedEightFramesAlongNextPath,
                      simulatedPathWithActionAfterArrival.squaredSpeedEightFramesAlongNextPath);
        };

        auto postValidateReturnPath =
                [&](auto &simulatedPathWithActionAtArrival,
                    auto &simulatedPathWithActionAfterArrival)
        {
            // For return there is nothing further to validate since the remaining fields are expected to differ
        };

        auto planPath = [&](auto &setArrivalData, auto &postValidatePath, bool isReturn)
        {
            followingPath = false;

            // Verify that the worker is always exactly facing the patch when it finishes mining
            // This is an assumption we use in our mining training
            if (isReturn)
            {
                auto expectedHeading = Geo::BWDirection(patch->getPosition() - worker->getPosition());
                EXPECT_EQ(worker->getExactPosition().heading, expectedHeading);
            }

            auto reset = [&]()
            {
                lastSimulationResult.reset();
                lastPrepareSimulationResult.reset();
                lastPrepareResult.reset();
            };

            auto assertEqual = [&](
                    const std::vector<BWAPI::ExactPosition> &expected, const std::vector<BWAPI::ExactPosition> &actual)
            {
                if (expected.size() != actual.size())
                {
                    EXPECT_EQ(expected.size(), actual.size()) << "Path size mismatch " << patch->getTilePosition();
                    return false;
                }
                for (size_t i = 0; i < expected.size(); i++)
                {
                    if (expected[i] != actual[i])
                    {
                        EXPECT_EQ(expected[i], actual[i]) << "Position mismatch " << i << " " << patch->getTilePosition();
                        return false;
                    }
                }
                return true;
            };

            auto simulate = [&](
                    const std::set<int> &resends,
                    std::optional<bool> forceAction = std::nullopt,
                    bool useLastSimulationResult = false,
                    bool includeStateCopy = false)
            {
                auto options = useLastSimulationResult
                        ? BWAPI::SimulateGatherPathOptions(resends, lastSimulationResult->stateAtStartOfNextPath)
                        : BWAPI::SimulateGatherPathOptions(resends);
                if (forceAction.has_value()) options.setForceAction(*forceAction);
                if (includeStateCopy) options.setReturnStateAtStartOfNextPath();

                auto result = worker->simulateGatherPath(options);
                if (!result)
                {
                    Log::Get() << "WARNING: Worker could not simulate path"
                               << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                    reset();
                }
                return result;
            };

            auto simulateFromPrepared = [&](const std::set<int> &resends, bool includeStateCopy = false)
            {
                int simStartFrame = lastPrepareSimulationResult ? lastPrepareSimulationResult->actionFrame : lastPrepareResult->returnPathStartFrame;
                std::set<int> modifiedResends;
                for (auto resend : resends)
                {
                    modifiedResends.insert(simStartFrame + (resend - currentFrame));
                }

                auto options = BWAPI::SimulateGatherPathOptions(
                        modifiedResends,
                        lastPrepareSimulationResult ? lastPrepareSimulationResult->stateAtStartOfNextPath : lastPrepareResult->returnPathState);
                if (includeStateCopy) options.setReturnStateAtStartOfNextPath();

                auto result = simWorker->simulateGatherPath(options);
                if (!result)
                {
                    Log::Get() << "WARNING: Sim worker could not simulate prepared path"
                               << "; patch " << patch->getID() << " @ " << patch->getTilePosition();
                    reset();
                }
                return result;
            };

            if (isReturn)
            {
                lastPrepareSimulationResult.reset();
                lastPrepareResult = simWorker->prepareGatherPath(
                        BWAPI::PrepareGatherPathOptions(worker->getExactPosition(), patch->getBWIndex(), initialState.state));
                if (!lastPrepareResult)
                {
                    Log::Get() << "WARNING: Sim worker could not prepare path"
                               << "; patch " << patch->getID() << " @ " << patch->getTilePosition();
                    reset();
                    return;
                }
                EXPECT_EQ(lastPrepareResult->returnPathStartPosition, worker->getExactPosition())
                    << "Prepared state does not have correct start position";
            }

            // Start by getting the path with no resends
            auto noResendPathResult = simulate({});
            if (!noResendPathResult) return;
            auto &noResendPath = noResendPathResult->positions;
            expectedPath.assign(noResendPath.begin(), noResendPath.end());
            setArrivalData(*noResendPathResult);

            // Validate that using the prepare method is equivalent
            if (lastPrepareResult || lastPrepareSimulationResult)
            {
                auto result = simulateFromPrepared({});
                if (!result) return;
                assertEqual(noResendPathResult->positions, result->positions);
            }

            // Pick the resend frames
            int arrivalFrame = currentFrame + (int)noResendPath.size();
            auto &chosenCombination = chooseResendCombination(arrivalFrame, isReturn ? 1 : 2);

            // As the simulate method only returns the positions from the last resend, we graft the full path together
            // We are taking advantage of the fact that std::set is sorted ascending by default
            plannedResendFrames.clear();
            for (auto &resendFrameDelta : chosenCombination)
            {
                int resendFrame = arrivalFrame + resendFrameDelta;
                if (resendFrame >= (currentFrame + (int)expectedPath.size()))
                {
                    // Expected path size can change along the way
                    reset();
                    return;
                }

                plannedResendFrames.insert(resendFrame);
                auto resendPathResult = simulate(plannedResendFrames);
                if (!resendPathResult) return;
                auto &resendPath = resendPathResult->positions;
                expectedPath.erase(expectedPath.begin() + (resendFrame - currentFrame), expectedPath.end());
                expectedPath.insert(expectedPath.end(), resendPath.begin(), resendPath.end());
                setArrivalData(*resendPathResult);

                // Validate that using the prepare method is equivalent
                if (lastPrepareResult || lastPrepareSimulationResult)
                {
                    auto result = simulateFromPrepared(plannedResendFrames);
                    if (!result) return;
                    if (!assertEqual(resendPath, result->positions))
                    {
                        simulateFromPrepared(plannedResendFrames);
                    }
                }
            }

            // Simulate the path again, forcing the action both ways, so we can validate the expected differences
            auto simulatedPathWhenTrue = simulate(plannedResendFrames, true);
            auto simulatedPathWhenFalse = simulate(plannedResendFrames, false);
            if (!simulatedPathWhenTrue || !simulatedPathWhenFalse) return;

            assertEqual(simulatedPathWhenTrue->positions, simulatedPathWhenFalse->positions);
            postValidatePath(*simulatedPathWhenTrue, *simulatedPathWhenFalse);

            // If we have a saved copy of the results of the previous path, validate that simulating from the state copy produces the same results
            if (lastSimulationResult)
            {
                auto simulatedPathFromCopyWhenTrue = simulate(plannedResendFrames, true, true);
                auto simulatedPathFromCopyWhenFalse = simulate(plannedResendFrames, false, true);
                if (!simulatedPathFromCopyWhenTrue || !simulatedPathFromCopyWhenFalse) return;

                auto assertResultFromCopy =
                        [&assertEqual](BWAPI::SimulateGatherPathResult &resultFromCopy, BWAPI::SimulateGatherPathResult &originalResult)
                {
                    assertEqual(resultFromCopy.positions, originalResult.positions);
                    EXPECT_EQ(resultFromCopy.actionPosition, originalResult.actionPosition);
                    EXPECT_EQ(resultFromCopy.nextPathStartPosition, originalResult.nextPathStartPosition);
                    EXPECT_EQ(resultFromCopy.squaredSpeedEightFramesAlongNextPath, originalResult.squaredSpeedEightFramesAlongNextPath);
                };
                assertResultFromCopy(*simulatedPathFromCopyWhenTrue, *simulatedPathWhenTrue);
                assertResultFromCopy(*simulatedPathFromCopyWhenFalse, *simulatedPathWhenFalse);
            }

            // Run the simulation one last time to save a copy of the result
            lastSimulationResult = simulate(plannedResendFrames, std::nullopt, false, true);

            // If on return path, swap out the state in our prepared data
            if (isReturn && lastPrepareResult)
            {
                lastPrepareSimulationResult = simulateFromPrepared(plannedResendFrames, true);
            }
//            lastPrepareResult.reset();

            std::ostringstream buf;
            std::string sep;
            for (auto frame : plannedResendFrames)
            {
                buf << sep << frame;
                sep = ", ";
            }
            CherryVis::log(worker->getID()) << "Planned path with resends at " << buf.str();

            followingPath = true;
        };

        auto validatePath = [&]()
        {
            if (!followingPath || expectedPath.empty()) return;

            if (worker->getExactPosition() != expectedPath.front())
            {
                Log::Get() << "ERROR: Worker not following expected path"
                           << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                ASSERT_EQ(worker->getExactPosition(), expectedPath.front());
            }
            else if ((expectedPath.front().heading != Geo::BWHeading(worker->getAngle()))
                || (expectedPath.front().velocityX != (int)std::round(worker->getVelocityX() * 256.0))
                || (expectedPath.front().velocityY != (int)std::round(worker->getVelocityY() * 256.0)))
            {
                Log::Get() << "ERROR: Worker BWAPI heading or velocity is wrong"
                           << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                ASSERT_EQ(expectedPath.front().heading, Geo::BWHeading(worker->getAngle()));
                ASSERT_EQ(expectedPath.front().velocityX, (int)std::round(worker->getVelocityX() * 256.0));
                ASSERT_EQ(expectedPath.front().velocityY, (int)std::round(worker->getVelocityY() * 256.0));
            }

            expectedPath.pop_front();
        };

        auto validateStartOfNextPath = [&](const std::set<PositionAndVelocity> &expectedNextPathStartPositions)
        {
            PositionAndVelocity actual(worker);
            if (expectedNextPathStartPositions.contains(actual)) return;

            Log::Get() << "ERROR: Next path start position does not match"
                       << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
            if (expectedNextPathStartPositions.size() == 1)
            {
                ASSERT_EQ(actual, *expectedNextPathStartPositions.begin());
            }
        };

        auto validateGatherFacingPatch = [&]()
        {
            if (!expectedGatherArrivalData) return;

            bool actualFacingPatch = (preMiningFrames == 1);
            if (actualFacingPatch != expectedGatherArrivalData->facingTarget())
            {
                Log::Get() << "ERROR: Facing patch does not match"
                           << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                ASSERT_EQ(actualFacingPatch, expectedGatherArrivalData->facingTarget());
            }
        };

        auto validateCollision = [&](bool expectedCollision)
        {
            // Resends early in the path can obscure the collision results, so don't validate this if there is a resend early in the path
            if (!plannedResendFrames.empty() && *plannedResendFrames.begin() < (currentFrame + 13)) return;

            // Assert the correct collision data
            bool collision = PathExplorationUtils::detectCollision(expectedPath);
            if (collision != expectedCollision)
            {
                Log::Get() << "ERROR: Collision mismatch, actual collision: " << collision
                           << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                ASSERT_EQ(collision, expectedCollision);
            }
        };

        auto validateGatherArrival = [&]()
        {
            if (!expectedGatherArrivalData) return;
            if (!followingPath) return; // require expected path for return

            validateStartOfNextPath({expectedGatherArrivalData->nextPathStartPosition});
            validateCollision(expectedGatherArrivalData->collision());
        };

        auto validateReturnArrival = [&]()
        {
            if (!expectedReturnArrivalData) return;
            if (!followingPath) return; // require expected path for gather
            if (expectedPath.size() < 8) return;

            validateStartOfNextPath({expectedReturnArrivalData->nextPathStartPositionDeliveryAtArrival,
                                     expectedReturnArrivalData->nextPathStartPositionDeliveryAfterArrival});

            auto expectedExitSpeed = expectedReturnArrivalData->exitSpeed();
            validateCollision(expectedExitSpeed == ReturnExitSpeed::Collision);
            if (expectedExitSpeed == ReturnExitSpeed::Collision) return;

            // Validate the exit speed, which is measured on the 8th position
            auto &exitSpeedPosition = expectedPath.at(7);
            auto velocityX = ((double)exitSpeedPosition.velocityX) / 256.0;
            auto velocityY = ((double)exitSpeedPosition.velocityY) / 256.0;
            auto speed = std::sqrt(velocityX * velocityX + velocityY * velocityY);
            ReturnExitSpeed actualExitSpeed = ReturnExitSpeed::Low;
            if (speed > 4.0)
            {
                actualExitSpeed = ReturnExitSpeed::High;
            }
            else if (speed > 2.5)
            {
                actualExitSpeed = ReturnExitSpeed::Medium;
            }
            if (actualExitSpeed != expectedExitSpeed)
            {
                Log::Get() << "ERROR: Exit speed mismatch, actual: " << actualExitSpeed
                           << "; expected: " << expectedExitSpeed
                           << "; actual speed: " << std::fixed << std::setprecision(3) << speed
                           << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
            }
        };

        auto issueResends = [&]()
        {
            if (!followingPath) return;
            if (!plannedResendFrames.contains(currentFrame + BWAPI::Broodwar->getLatencyFrames())) return;

            auto result = (state == 0) ? worker->gather(patch) : worker->rightClick(depot);
            if (!result)
            {
                Log::Get() << "WARNING: Worker could not issue resend: " << BWAPI::Broodwar->getLastError()
                           << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                followingPath = false;
                return;
            }
            CherryVis::log(worker->getID()) << "Reissued command";
        };

        switch (state)
        {
            case 0:
            {
                // Worker is approaching the patch; transition to state 1 when it starts mining
                if (worker->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    CherryVis::log(worker->getID()) << "State transition from approaching patch to pre-mining";
                    state = 1;
                    preMiningFrames = 1;
                    return;
                }

                validatePath();
                issueResends();

                return;
            }
            case 1:
            {
                if (worker->getOrderTimer() == 1)
                {
                    CherryVis::log(worker->getID()) << "State transition from pre-mining to mining";
                    state = 2;
                    validateGatherFacingPatch();
                    return;
                }

                preMiningFrames++;

                return;
            }
            case 2:
            {
                // Worker is mining; transition to state 2 when it is finished mining
                if (worker->getOrder() == BWAPI::Orders::ReturnMinerals && worker->isCarryingMinerals())
                {
                    CherryVis::log(worker->getID()) << "State transition from mining to returning minerals";
                    state = 3;

                    planPath(setReturnArrivalData, postValidateReturnPath, true);
                    if (!followingPath)
                    {
                        expectedReturnArrivalData.reset();
                    }

                    validateGatherArrival();

                    // validate
                    issueResends();
                }

                return;
            }
            case 3:
            {
                // Worker is returning minerals; transition to state 0 when it has returned minerals
                if (!worker->isCarryingMinerals())
                {
                    CherryVis::log(worker->getID()) << "State transition from returning minerals to approaching patch";
                    state = 0;

                    planPath(setGatherArrivalData, postValidateGatherPath, false);
                    if (!followingPath)
                    {
                        expectedGatherArrivalData.reset();
                    }

                    validateReturnArrival();

                    issueResends();
                    return;
                }

                validatePath();
                issueResends();

                return;
            }
            default:
            {
                Log::Get() << "ERROR: Worker has unknown state " << state;
                return;
            }
        }
    }
}
