#pragma once

#include "Common.h"
#include "Unit.h"

namespace NoGoAreas
{
    void initialize();

    void update();

    void writeInstrumentation();

    bool isNoGo(BWAPI::TilePosition pos);

    bool isNoGo(int x, int y);

    // Adds a box-shaped no-go area that must be manually removed by calling removeBox
    void addBox(BWAPI::TilePosition topLeft, BWAPI::TilePosition size);

    // Removes a box-shaped no-go area
    void removeBox(BWAPI::TilePosition topLeft, BWAPI::TilePosition size);

    // Adds a circular no-go area from the given point with the given radius, that expires after the given number of frames
    void addCircle(BWAPI::Position origin, int radius, int expireFrames);

    // Adds a circular no-go area from the given point with the given radius, that expires when the given bullet no longer exists
    void addCircle(BWAPI::Position origin, int radius, BWAPI::Bullet bullet);

    // Adds a box from the origin to the target with the given width. that expires after the given number of frames
    void addDirectedBox(BWAPI::Position origin, BWAPI::Position target, int width, int expireFrames);

    // Adds a box from the origin to the target with the given width. that expires after the given bullet no longer exists
    void addDirectedBox(BWAPI::Position origin, BWAPI::Position target, int width, BWAPI::Bullet bullet);

    void onBulletCreate(BWAPI::Bullet bullet);
}