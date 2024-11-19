#include "BWAPI/GameImpl.h"
#include "gtest/gtest.h"
#include "DumpReplayDataModule.h"
#include <execinfo.h>

TEST(DumpReplayData, DumpReplayData)
{
    auto replayFile = "/Users/bruce.nielsen/BW/desyncs/22-Iron Bot-GAME_B3681C65.rep";

    BW::GameOwner gameOwner;
    BWAPI::BroodwarImpl_handle h(gameOwner.getGame());
    BWAPI::BroodwarImpl.bwgame.setMapFileName(replayFile);

    h->createSinglePlayerGame([&]()
                             {
                                 h->startGame();
                             });

    auto module = DumpReplayDataModule("openbw");
    h->setAIModule(&module);

    while (!gameOwner.getGame().gameOver())
    {
        h->update();
        gameOwner.getGame().nextFrame();
    }
}
