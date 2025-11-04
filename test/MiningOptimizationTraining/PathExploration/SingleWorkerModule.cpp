#include "SingleWorkerModule.h"

namespace MiningOptimizationTraining
{
    namespace
    {
        std::vector<BWAPI::Position> noResendPath = {
                {240,296},
                {240,296},
                {240,296},
                {240,296},
                {240,296},
                {240,296},
                {239,295},
                {239,295},
                {238,295},
                {237,295},
                {236,295},
                {234,294},
                {232,294},
                {230,294},
                {228,293},
                {225,292},
                {223,292},
                {220,291},
                {217,289},
                {215,286},
                {212,283},
                {209,280},
                {205,277},
                {202,274},
                {199,270},
                {195,267},
                {191,263},
                {188,260},
                {184,256},
                {181,253},
                {177,249},
                {174,246},
                {170,242},
                {167,239},
                {163,235},
                {160,231},
                {156,228},
                {156,227},
                {152,223},
                {148,220},
                {144,217},
                {141,213},
                {139,212},
                {134,212},
                {129,212},
                {125,212},
                {121,212},
                {117,212},
                {114,212},
                {111,212},
                {108,212}
        };

        std::vector<BWAPI::ExactPosition> actuals;
    }
    bool SingleWorkerModule::initialize()
    {
        if (BWAPI::Broodwar->getFrameCount() == 0)
        {
            // Kill all but the leftmost initial worker
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType().isWorker() && unit->getPosition() != BWAPI::Position(240, 296))
                {
                    BWAPI::Broodwar->killUnit(unit);
                }
            }
        }

        // Give the workers time to die
        if (BWAPI::Broodwar->getFrameCount() < 10) return false;

        // Find the worker and depot
        BWAPI::Unit worker;
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType().isWorker()) worker = unit;
        }
        if (!worker) return false;

        // Find the patch
        auto patch = (*Map::getMyMain()->mineralPatches().begin())->getBwapiUnitIfVisible();
        if (!patch) return false;

        // Leave when the worker arrives at the patch
//        if (worker->getDistance(patch) == 0)
//        {
//            BWAPI::Broodwar->leaveGame();
//            return false;
//        }

        // Send the command on frame 10 and the resend frame
        if (BWAPI::Broodwar->getFrameCount() == 10 || BWAPI::Broodwar->getFrameCount() == resendFrame)
        {
            if (BWAPI::Broodwar->getFrameCount() == resendFrame)
            {
                Log::Get() << "Resending at frame " << resendFrame;
            }
            worker->gather(patch);
        }

        auto out = [](const auto &container)
        {
            std::ostringstream buf;
            std::string sep;
            for (const auto &item : container)
            {
                buf << sep << item;
                sep = ", ";
            }
            return buf.str();
        };

        // Resend return minerals
        if (BWAPI::Broodwar->getFrameCount() == 80)
        {
            auto result = worker->simulateGatherPath({107});
            if (result.has_value())
            {
                Log::Get() << "Simulated: " << out(result->first);
            }
        }
        if (BWAPI::Broodwar->getFrameCount() > 80)
        {
            if (BWAPI::Broodwar->getFrameCount() == 105)
            {
                worker->returnCargo();
            }
            if (BWAPI::Broodwar->getFrameCount() < 130)
            {
                actuals.push_back(worker->getExactPosition());
            }
            else if (BWAPI::Broodwar->getFrameCount() == 130)
            {
                Log::Get() << "   Actual: " << out(actuals);
            }
        }

        if (resendFrame == -1)
        {
            auto resendChanges = worker->wouldAGatherResendHereChangeThePath();
            if (resendChanges.has_value())
            {
                Log::Get() << worker->getPosition() << ": resend changes: " << resendChanges.value();
            }
            else
            {
                Log::Get() << worker->getPosition() << ": resend changes: unknown";
            }

            auto simulatedPaths = worker->simulatePathWithAndWithoutResend();
            auto pathToString = [](const std::vector<std::pair<int, int>> &path)
            {
                std::ostringstream buf;
                std::string sep;
                for (const auto &[x, y] : path)
                {
                    buf << sep << "(" << x << "," << y << ")";
                    sep = ", ";
                }
                return buf.str();
            };
            auto pathsEqual = [](const std::vector<std::pair<int, int>> &a, const std::vector<std::pair<int, int>> &b)
            {
                if (a.size() != b.size()) return false;
                auto a_it = a.begin();
                auto b_it = b.begin();
                while (a_it != a.end())
                {
                    if (a_it->first != b_it->first || a_it->second != b_it->second) return false;

                    a_it++;
                    b_it++;
                }
                return true;
            };
            if (simulatedPaths.has_value())
            {
                Log::Get() << worker->getPosition() << ": simulated paths equal: " << pathsEqual(simulatedPaths->first, simulatedPaths->second);
                Log::Get() << worker->getPosition() << ": resend path: " << pathToString(simulatedPaths->first);
                Log::Get() << worker->getPosition() << ": no-resend path: " << pathToString(simulatedPaths->second);
            }
            else
            {
                Log::Get() << worker->getPosition() << ": Unknown path simulation";
            }
            return false;
        }

//        auto index = BWAPI::Broodwar->getFrameCount() - 10;
//        if (index >= noResendPath.size())
//        {
//            Log::Get() << "Path diverged - expected to be finished here";
//        }
//        else if (noResendPath[index] != worker->getPosition())
//        {
//            Log::Get() << "Path diverged: " << worker->getPosition() << " vs. expected " << noResendPath[index];
//        }

        return false;
    }
}
