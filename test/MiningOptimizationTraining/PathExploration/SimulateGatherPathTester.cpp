#include "SimulateGatherPathTester.h"

#include "BWAPI/SimulateGatherPathOptions.h"
#include "BWAPI/SimulateGatherPathResult.h"

#include "Geo.h"

#include <random>

namespace MiningOptimizationTraining
{
    namespace
    {
        std::default_random_engine rng(42); // NOLINT(*-msc51-cpp) - using fixed sequence for reproducability
        std::vector<std::set<int>> resendCombinations;

        std::set<int> &chooseResendCombination(int arrivalFrame)
        {
            // Initialize resend combinations lazily
            if (resendCombinations.empty())
            {
                resendCombinations.emplace_back(); // the no resend option
                for (int firstResend = -50; firstResend <= -5; firstResend++)
                {
                    resendCombinations.push_back({firstResend});
                    for (int secondResend = (firstResend + 1); secondResend <= -5; secondResend++)
                    {
                        if (secondResend == (firstResend + BWAPI::Broodwar->getLatencyFrames())) continue;
                        resendCombinations.push_back({firstResend, secondResend});
                    }
                }
            }

            while (true)
            {
                // Pick a combination
                std::uniform_int_distribution<size_t> dist(0, resendCombinations.size() - 1);
                auto &chosenCombination = resendCombinations[dist(rng)];

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

        auto planPath = [&](auto &setArrivalData, auto &postValidatePath)
        {
            followingPath = false;

            // Start by getting the path with no resends
            auto noResendPathResult = worker->simulateGatherPath({});
            if (!noResendPathResult)
            {
                Log::Get() << "WARNING: Worker could not plan path"
                           << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                return;
            }
            auto &noResendPath = noResendPathResult->positions;
            expectedPath.assign(noResendPath.begin(), noResendPath.end());
            setArrivalData(*noResendPathResult);

            // Pick the resend frames
            int arrivalFrame = currentFrame + (int)noResendPath.size();
            auto &chosenCombination = chooseResendCombination(arrivalFrame);

            // As the simulate method only returns the positions from the last resend, we graft the full path together
            // We are taking advantage of the fact that std::set is sorted ascending by default
            plannedResendFrames.clear();
            for (auto &resendFrameDelta : chosenCombination)
            {
                int resendFrame = arrivalFrame + resendFrameDelta;
                if (resendFrame >= (currentFrame + (int)expectedPath.size())) return; // Expected path size can change along the way

                plannedResendFrames.insert(resendFrame);
                auto resendPathResult = worker->simulateGatherPath(BWAPI::SimulateGatherPathOptions(plannedResendFrames));
                if (!resendPathResult)
                {
                    Log::Get() << "WARNING: Worker could not plan path"
                               << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                    return;
                }
                auto &resendPath = resendPathResult->positions;
                expectedPath.erase(expectedPath.begin() + (resendFrame - currentFrame), expectedPath.end());
                expectedPath.insert(expectedPath.end(), resendPath.begin(), resendPath.end());
                setArrivalData(*resendPathResult);
            }

            // Simulate the path again, forcing the action both ways, so we can validate the expected differences
            auto simulatedPathWhenTrue =
                    worker->simulateGatherPath(BWAPI::SimulateGatherPathOptions(plannedResendFrames).setForceAction(true));
            auto simulatedPathWhenFalse =
                    worker->simulateGatherPath(BWAPI::SimulateGatherPathOptions(plannedResendFrames).setForceAction(false));
            if (!simulatedPathWhenTrue || !simulatedPathWhenTrue)
            {
                Log::Get() << "WARNING: Worker could not plan path"
                           << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                return;
            }
            auto &firstPath = simulatedPathWhenTrue->positions;
            auto &secondPath = simulatedPathWhenFalse->positions;
            EXPECT_EQ(firstPath.size(), secondPath.size());
            if (firstPath.size() == secondPath.size())
            {
                for (size_t i = 0; i < firstPath.size(); i++)
                {
                    EXPECT_EQ(firstPath[i], secondPath[i]);
                }
            }
            postValidatePath(*simulatedPathWhenTrue, *simulatedPathWhenFalse);

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

            // Go through the start of the path to see if the worker stalled for a full order process timer cycle
            int stallFrames = 0;
            int maxStallFrames = 0;
            size_t maxStallStart = 0;
            for (auto it = expectedPath.begin() + 1; it != expectedPath.end() && std::distance(expectedPath.begin(), it) < 13; it++)
            {
                if ((*it).pos() == (*(it - 1)).pos())
                {
                    stallFrames++;
                    if (stallFrames > maxStallFrames)
                    {
                        maxStallFrames = stallFrames;
                        maxStallStart = std::distance(expectedPath.begin(), it) - maxStallFrames;
                    }
                }
                else
                {
                    stallFrames = 0;
                }
            }
            bool collision = (maxStallFrames > 7);

            // Assert the correct collision data
            if (collision != expectedCollision)
            {
                Log::Get() << "ERROR: Collision mismatch, actual collision: " << collision
                           << "; stall frames: " << maxStallFrames
                           << "; stall start " << maxStallStart
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

                    planPath(setReturnArrivalData, postValidateReturnPath);
                    if (!followingPath) expectedReturnArrivalData.reset();

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

                    planPath(setGatherArrivalData, postValidateGatherPath);
                    if (!followingPath) expectedGatherArrivalData.reset();

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
