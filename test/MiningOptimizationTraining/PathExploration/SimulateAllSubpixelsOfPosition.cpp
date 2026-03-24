#include "ExploreStartPositionsModule.h"

#include <BWAPI/SimulateGatherPathOptions.h>
#include <BWAPI/SimulateGatherPathResult.h>

namespace MiningOptimizationTraining
{
    namespace
    {
        std::vector<std::pair<size_t, BWAPI::ExactPosition>> foundSolutions;
        std::array<size_t, 256*256> solutions;
        int count = 0;

        template <class T>
        inline void hash_combine(std::size_t& seed, const T& v)
        {
            std::hash<T> hasher;
            seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
        }
    }

    template <>
    void ExploreStartPositionsModule<SimulateAllSubpixelsOfPosition>::initializeStartPositions()
    {
        BWAPI::Unit patchUnit;
        for (auto unit : BWAPI::Broodwar->getNeutralUnits())
        {
            if (unit->getType().isMineralField() && unit->getTilePosition() == BWAPI::TilePosition(1, 6))
            {
                patchUnit = unit;
            }
        }

        unsigned int baseX = (107 << 8);
        unsigned int baseY = (203 << 8);
        for (unsigned int subpixelX = 0; subpixelX < 256; subpixelX++)
        {
            for (unsigned int subpixelY = 0; subpixelY < 256; subpixelY++)
            {
                startPositions.emplace_back(SimulateAllSubpixelsOfPosition{
                        BWAPI::ExactPosition{baseX + subpixelX, baseY + subpixelY, -68, 0, 0},
                        patchUnit});
            }
        }
    }

    template <>
    void ExploreStartPositionsModule<SimulateAllSubpixelsOfPosition>::explore(SimulateAllSubpixelsOfPosition &startPosition)
    {
        auto preparedReturnPath = prepareReturnPath(startPosition, initialStateWithNoCannons);
        if (!preparedReturnPath) return;

        auto returnResult = simWorker->simulateGatherPath(
                BWAPI::SimulateGatherPathOptions({}, preparedReturnPath->returnPathState));
        if (!returnResult)
        {
            Log::Get() << "Failed to simulate";
            return;
        }

        size_t hash = returnResult->positions.size();
        for (auto &pos : returnResult->positions)
        {
            auto bwapiPos = pos.pos();
            hash_combine(hash, bwapiPos.x);
            hash_combine(hash, bwapiPos.y);
        }

        auto result =
                std::make_pair<size_t, BWAPI::ExactPosition>(std::move(hash), std::move(returnResult->nextPathStartPosition));
        size_t match;
        for (match = 0; match < foundSolutions.size(); match++)
        {
            if (foundSolutions[match] == result)
            {
                break;
            }
        }
        if (match == foundSolutions.size())
        {
            foundSolutions.emplace_back(std::move(result));
        }

        int i = (startPosition.pos.y % 256) * 256 + (startPosition.pos.x % 256);
        solutions[i] = match;

        if (++count == (256*256))
        {
            Log::Get() << foundSolutions.size() << " unique solutions";
            std::ostringstream buf;
            for (int y = 0; y < 256; y++)
            {
                for (int x = 0; x < 256; x++)
                {
                    buf << solutions[y * 256 + x] << " ";
                }
                buf << "\n";
            }
            Log::Get() << "\n" << buf.str();

            std::set<size_t> foundResendSolutions;
            unsigned int baseX = (107 << 8);
            unsigned int baseY = (203 << 8);
            int resendCount = 0;
            for (int y = 0; y < 256; y++)
            {
                for (int x = 0; x < 256; x++)
                {
                    if (solutions[y * 256 + x] == 0)
                    {
                        resendCount++;
                        auto resendStartPos = BWAPI::ExactPosition{baseX + x, baseY + y, -68, 0, 0};

                        auto prepareResult = simWorker->prepareGatherPath(
                                BWAPI::PrepareGatherPathOptions(resendStartPos, startPosition.patch->getBWIndex(), initialStateWithNoCannons.state));
                        if (!prepareResult)
                        {
                            Log::Get() << "ERROR: Failed to prepare gather path";
                            return;
                        }

                        size_t resendHash = 0;
                        for (int resend = 20; resend < 50; resend++)
                        {
                            auto resendReturnResult = simWorker->simulateGatherPath(
                                    BWAPI::SimulateGatherPathOptions({resend}, prepareResult->returnPathState));
                            if (!resendReturnResult)
                            {
                                Log::Get() << "Failed to simulate";
                                return;
                            }

                            hash_combine(resendHash, resendReturnResult->positions.size());
                            for (auto &pos : resendReturnResult->positions)
                            {
                                auto bwapiPos = pos.pos();
                                hash_combine(resendHash, bwapiPos.x);
                                hash_combine(resendHash, bwapiPos.y);
                            }
                            foundResendSolutions.insert(resendHash);
                        }
                    }
                }
            }
            Log::Get() << foundResendSolutions.size() << " resend solution(s) found out of " << resendCount << " with same no-resend result";
        }
    }
}
