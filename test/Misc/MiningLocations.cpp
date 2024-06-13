#include "BWTest.h"
#include "DoNothingModule.h"

namespace
{
    class MineLocationsModule : public DoNothingModule
    {
    private:
        BWAPI::Unit worker = nullptr;
        int state = 0;

    public:
        void onFrame() override
        {
            if (BWAPI::Broodwar->getFrameCount() > 2000)
            {
                BWAPI::Broodwar->leaveGame();
                return;
            }

            // OK tile
//            auto patchTile = BWAPI::TilePosition(9, 37);

            // Problematic tile
            auto patchTile = BWAPI::TilePosition(10, 44);

            auto holdingPosition = BWAPI::Position(patchTile) + BWAPI::Position(32, -20);
            auto miningStartPosition = BWAPI::Position(patchTile) + BWAPI::Position(32, -12);

            if (!worker)
            {
                for (auto &unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType().isWorker())
                    {
                        worker = unit;
                        break;
                    }
                }

                if (!worker) return;
            }

            BWAPI::Unit patch = nullptr;
            for (auto &unit : BWAPI::Broodwar->getStaticNeutralUnits())
            {
                if (unit->getTilePosition() == patchTile)
                {
                    patch = unit;
                    break;
                }
            }

            if (BWAPI::Broodwar->getFrameCount() < 1500)
            {
                if (worker->getLastCommand().getType() != BWAPI::UnitCommandTypes::Move)
                {
                    worker->move(holdingPosition);
                }

                if (state == 0 && worker->getPosition() == holdingPosition)
                {
                    state = 1;
                    worker->move(miningStartPosition);
                }

                if (patch && patch->isVisible())
                {
                    std::cout << BWAPI::Broodwar->getFrameCount()
                              << ": pos=" << worker->getPosition()
                              << "; movingTo=" << (state == 0 ? holdingPosition : miningStartPosition)
                              << "; patchDist=" << patch->getDistance(worker)
                              << std::endl;
                }
                else
                {
                    std::cout << BWAPI::Broodwar->getFrameCount()
                              << ": pos=" << worker->getPosition()
                              << "; movingTo=" << (state == 0 ? holdingPosition : miningStartPosition)
                              << "; patchDist=(not yet visible)"
                              << std::endl;
                }
            }
            else
            {
                if (patch && worker->getLastCommand().getType() != BWAPI::UnitCommandTypes::Gather)
                {
                    worker->gather(patch);
                }

                std::cout << BWAPI::Broodwar->getFrameCount()
                          << ": pos=" << worker->getPosition()
                          << "; gathering"
                          << std::endl;
            }
        }
    };
}

TEST(MiningLocations, FightingSpirit)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.myModule = []()
    {
        return new MineLocationsModule();
    };
    test.map = Maps::GetOne("Fighting");
    test.frameLimit = 5000;
    test.expectWin = false;
    test.writeReplay = true;
    test.run();
}