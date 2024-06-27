#include "BWTest.h"
#include "DoNothingModule.h"
#include "InstrumentedDoNothingModule.h"

#include <ranges>
#include <bwem.h>

#include "PatchAnalysis.h"
#include "RemoveOpponentDepotModule.h"

#include "Geo.h"

namespace
{
    const std::string dataBasePath = "/Users/bmnielsen/BW/mining-timings/";

    std::vector<BWAPI::TilePosition> getAllPatches(BWTest &test)
    {
        test.opponentModule = test.myModule = []()
        {
            return new DoNothingModule();
        };
        test.frameLimit = 10;
        test.expectWin = false;
        test.writeReplay = false;

        std::vector<BWAPI::TilePosition> result;
        test.onStartMine = [&result]()
        {
            for (auto unit : BWAPI::Broodwar->getNeutralUnits())
            {
                if (!unit->getType().isMineralField()) continue;
                if (unit->getInitialResources() < 200) continue;

                result.emplace_back(unit->getTilePosition());
            }
        };

        test.run();

        return result;
    }

    class PatchAnalysisModule : public InstrumentedDoNothingModule
    {
        std::vector<BWAPI::TilePosition> patchTiles;
        int batch;

        std::vector<PatchAnalysis> patchAnalysis;

    public:
        explicit PatchAnalysisModule(const std::vector<BWAPI::TilePosition> &patchTiles, int batch)
                : patchTiles(patchTiles)
                , batch(batch)
        {}

        void onStart() override
        {
            InstrumentedDoNothingModule::onStart();

            BWEM::Map::ResetInstance();
            BWEM::Map::Instance().Initialize(BWAPI::BroodwarPtr);
            BWEM::Map::Instance().EnableAutomaticPathAnalysis();
            BWEM::Map::Instance().FindBasesForStartingLocations();

            // Kill our starting workers so they can't get in the way
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType().isWorker())
                {
                    BWAPI::Broodwar->killUnit(unit);
                }
            }

            // Initialize all patches we want to analyze
            for (const auto &patchTile : patchTiles)
            {
                PatchAnalysis patch(patchTile);

                bool hasDepotAlready = false;
                for (const auto &other : patchAnalysis)
                {
                    if (patch.depotTile == other.depotTile)
                    {
                        hasDepotAlready = true;
                        break;
                    }
                }
                if (hasDepotAlready) continue;

                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, patch.depotCenter());

                patchAnalysis.emplace_back(std::move(patch));
            }

            Log::Get() << "Initialized test; ready to optimize " << patchAnalysis.size() << " patch(es)";
        }

        void onFrame() override
        {
            InstrumentedDoNothingModule::onFrameStart();

            // Give initial workers time to be killed
            if (currentFrame < 10) return;

            if (patchAnalysis.empty())
            {
                BWAPI::Broodwar->leaveGame();
                InstrumentedDoNothingModule::onFrameEnd();
                return;
            }

            for (auto it = patchAnalysis.begin(); it != patchAnalysis.end();)
            {
                if (it->complete)
                {
                    it = patchAnalysis.erase(it);
                }
                else
                {
                    it->onFrame(batch, dataBasePath);
                    it++;
                }
            }

            InstrumentedDoNothingModule::onFrameEnd();
        }
    };

    void runPatchAnalysis(BWTest &test)
    {
        auto patches = getAllPatches(test);

        PatchAnalysis::clearOutputFiles(dataBasePath, test.map->openbwHash);

        test.opponentModule = []()
        {
            return new RemoveOpponentDepotModule();
        };
        test.opponentRace = BWAPI::Races::Protoss;
        test.frameLimit = 200000;
        test.randomSeed = 42;
        test.timeLimit = 600;
        test.expectWin = false;
        test.writeReplay = true;
        test.onStartMine = nullptr;

        // Repeatedly run until all patches have been analyzed
        int batch = 0;
        std::string baseReplayName = test.replayName;
        do
        {
            batch++;

            std::ostringstream replayName;
            replayName << baseReplayName;
            replayName << "_batch_" << batch;
            test.replayName = replayName.str();

            test.myModule = [&patches, &batch]()
            {
                return new PatchAnalysisModule(patches, batch);
            };

            std::cout << "Starting analysis game " << batch << " with " << patches.size() << " patch(es) remaining" << std::endl;
            test.run();

            // Remove patches that have been analyzed
            std::set<BWAPI::TilePosition> analyzedPatches = PatchAnalysis::getAnalyzedPatches(dataBasePath, test.map->openbwHash);
            for (auto it = patches.begin(); it != patches.end(); )
            {
                if (analyzedPatches.contains(*it))
                {
                    it = patches.erase(it);
                }
                else
                {
                    it++;
                }
            }
        } while (!patches.empty());
    }
}

TEST(OptimizeMining, OptimizeAllPatches_FightingSpirit)
{
    Maps::RunOnEach(Maps::Get("sscai/(4)Fighting"), [](BWTest test)
    {
        runPatchAnalysis(test);
    });
}

TEST(OptimizeMining, OptimizeAllPatches_AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [](BWTest test)
    {
        runPatchAnalysis(test);
    });
}
