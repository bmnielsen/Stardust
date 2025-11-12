#include "SimulateGatherPathTester.h"

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
        auto planPath = [&]()
        {
            followingPath = false;

            auto noResendPathResult = worker->simulateGatherPath({});
            if (!noResendPathResult.has_value())
            {
                Log::Get() << "WARNING: Worker could not plan path"
                           << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                return;
            }
            auto &noResendPath = std::get<0>(*noResendPathResult);
            expectedPath.assign(noResendPath.begin(), noResendPath.end());
            expectedNextPathStartPosition = std::get<1>(*noResendPathResult);

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
                auto resendPathResult = worker->simulateGatherPath(plannedResendFrames);
                if (!noResendPathResult.has_value())
                {
                    Log::Get() << "WARNING: Worker could not plan path"
                               << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                    return;
                }
                auto &resendPath = std::get<0>(*resendPathResult);
                expectedPath.erase(expectedPath.begin() + (resendFrame - currentFrame), expectedPath.end());
                expectedPath.insert(expectedPath.end(), resendPath.begin(), resendPath.end());
                expectedNextPathStartPosition = std::get<1>(*resendPathResult);
            }

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

            expectedPath.pop_front();
        };

        auto validateStartOfNextPath = [&]()
        {
            if (!followingPath) return;

            if (worker->getExactPosition() != expectedNextPathStartPosition)
            {
                Log::Get() << "ERROR: Next path start position does not match"
                           << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                ASSERT_EQ(worker->getExactPosition(), expectedNextPathStartPosition);
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
                    CherryVis::log(worker->getID()) << "State transition from approaching patch to mining";
                    state = 1;
                    return;
                }

                validatePath();
                issueResends();

                return;
            }
            case 1:
            {
                // Worker is mining; transition to state 2 when it is finished mining
                if (worker->getOrder() == BWAPI::Orders::ReturnMinerals && worker->isCarryingMinerals())
                {
                    CherryVis::log(worker->getID()) << "State transition from mining to returning minerals";
                    state = 2;
                    validateStartOfNextPath();
                    planPath();
                    issueResends();
                }

                return;
            }
            case 2:
            {
                // Worker is returning minerals; transition to state 0 when it has returned minerals
                if (!worker->isCarryingMinerals())
                {
                    CherryVis::log(worker->getID()) << "State transition from returning minerals to approaching patch";
                    state = 0;
                    validateStartOfNextPath();
                    planPath();
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
