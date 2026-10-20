#pragma once

#include "Common.h"

class ShushanKingAIModule : public BWAPI::AIModule
{
public:
    /* Used in our test infrastructure */
    int frameSkip = 0;
    std::function<void()> testOnStart = nullptr;
    std::function<void()> testOnFrame = nullptr;
    std::function<void(bool)> testOnEnd = nullptr;

    /* Used for getting stable pointers in the case of crashes */
    unsigned long moduleBaseAddress = 0;

    /* Whether to surrender when losing is inevitable */
    bool enableSurrender = false;

    bool enableFrameLimit = true;

    void onStart() override;

    void onEnd(bool isWinner) override;

    void onFrame() override;

    /* Original onFrame body; called under the SEH crash guard (B7) and the */
    /* A4 frame governor (S5) from onFrame(). runHeavy=false yields the heavy */
    /* decision layers while the pump (events + unit bookkeeping) still runs. */
    void onFrameGuarded(bool runHeavy);

    void onSendText(std::string text) override;

    void onReceiveText(BWAPI::Player player, std::string text) override;

    void onPlayerLeft(BWAPI::Player player) override;

    void onNukeDetect(BWAPI::Position target) override;

    void onUnitDiscover(BWAPI::Unit gameUnit) override;

    void onUnitEvade(BWAPI::Unit gameUnit) override;

    void onUnitShow(BWAPI::Unit gameUnit) override;

    void onUnitHide(BWAPI::Unit gameUnit) override;

    void onUnitCreate(BWAPI::Unit gameUnit) override;

    void onUnitDestroy(BWAPI::Unit gameUnit) override;

    void onUnitMorph(BWAPI::Unit gameUnit) override;

    void onUnitRenegade(BWAPI::Unit gameUnit) override;

    void onSaveGame(std::string gameName) override;

    void onUnitComplete(BWAPI::Unit gameUnit) override;
};
