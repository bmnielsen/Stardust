#pragma once
#include <BWAPI.h>

namespace McRave::Visuals {

    void onFrame();
    void startPerfTest();
    void endPerfTest(std::string);
    void onSendText(std::string);
    void drawPath(BWEB::Path&);

    void centerCameraOn(BWAPI::Position);


    void drawDebugText(std::string, double);
    void drawDebugText(std::string, int);

    void drawBox(BWAPI::Position, BWAPI::Position, BWAPI::Color, bool solid = false);
    void drawBox(BWAPI::WalkPosition, BWAPI::WalkPosition, BWAPI::Color, bool solid = false);
    void drawBox(BWAPI::TilePosition, BWAPI::TilePosition, BWAPI::Color, bool solid = false);

    void drawCircle(BWAPI::Position, int, BWAPI::Color, bool solid = false);
    void drawCircle(BWAPI::WalkPosition, int, BWAPI::Color, bool solid = false);
    void drawCircle(BWAPI::TilePosition, int, BWAPI::Color, bool solid = false);

    void drawLine(const BWEM::ChokePoint *, BWAPI::Color);
    void drawLine(BWAPI::Position, BWAPI::Position, BWAPI::Color);
    void drawLine(BWAPI::WalkPosition, BWAPI::WalkPosition, BWAPI::Color);
    void drawLine(BWAPI::TilePosition, BWAPI::TilePosition, BWAPI::Color);
};