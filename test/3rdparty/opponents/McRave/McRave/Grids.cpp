#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Grids
{
    namespace {

        vector<WalkPosition> resetVector;

        bool resetGrid[1024][1024] ={};
        int visibleGrid[256][256] ={};
        int visitedGrid[1024][1024] ={};

        // Ally Grid
        float aGroundCluster[1024][1024] ={};
        float aAirCluster[1024][1024] ={};

        // Enemy Grid
        float eGroundThreat[1024][1024] ={};
        float eAirThreat[1024][1024] ={};
        float eGroundCluster[1024][1024] ={};
        float eAirCluster[1024][1024] ={};
        int eSplash[1024][1024] ={};

        // Mobility Grid
        int mobility[1024][1024] ={};
        int collision[1024][1024] ={};
        int horizontalCollision[1024][1024] ={};
        int verticalCollision[1024][1024] ={};

        bool cliffVision[256][256] ={};

        int fasterDistGrids(int x1, int y1, int x2, int y2) {
            unsigned int min = abs((int)(x1 - x2));
            unsigned int max = abs((int)(y1 - y2));
            if (max < min)
                std::swap(min, max);

            if (min < (max >> 2))
                return max;

            unsigned int minCalc = (3 * min) >> 3;
            return (minCalc >> 5) + minCalc + max - (max >> 4) - (max >> 6);
        }

        void saveReset(int x, int y)
        {
            if (!std::exchange(resetGrid[x][y], 1))
                resetVector.emplace_back(x, y);
        }

        void addSplash(UnitInfo& unit)
        {
            WalkPosition start(unit.getTarget().getWalkPosition());
            Position target = unit.getTarget().getPosition();

            for (int x = start.x - 12; x <= start.x + 12 + unit.getWalkWidth(); x++) {
                for (int y = start.y - 12; y <= start.y + 12 + unit.getWalkHeight(); y++) {

                    WalkPosition w(x, y);
                    Position p = Position(w) + Position(4, 4);
                    if (!w.isValid())
                        continue;

                    saveReset(x, y);
                    eSplash[x][y] += (target.getDistance(p) <= 96);
                }
            }
        }

        void addToGrids(UnitInfo& unit, Position position, WalkPosition walkPosition)
        {
            // Pixel and walk sizes
            const auto walkWidth = unit.getType().isBuilding() ? (unit.getType().tileWidth() * 4) : unit.getWalkWidth();
            const auto walkHeight = unit.getType().isBuilding() ? (unit.getType().tileHeight() * 4) : unit.getWalkHeight();

            // Choose threat grid
            auto grdGrid = unit.getPlayer() == Broodwar->self() ? nullptr : eGroundThreat;
            auto airGrid = unit.getPlayer() == Broodwar->self() ? nullptr : eAirThreat;

            // Choose cluster grid
            auto clusterGrid = unit.getPlayer() == Broodwar->self() ?
                (unit.getType().isFlyer() ? aAirCluster : aGroundCluster) :
                (unit.getType().isFlyer() ? eAirCluster : eGroundCluster);

            // Limit checks so we don't have to check validity
            auto radius = unit.getPlayer() != Broodwar->self() ? 1 + int(max(unit.getGroundReach(), unit.getAirReach())) / 8 : 8;

            if (unit.getType().isWorker() &&
                (unit.unit()->isConstructing() || unit.unit()->isGatheringGas() || unit.unit()->isGatheringMinerals()))
                radius = radius / 3;

            const auto left = max(0, walkPosition.x - radius);
            const auto right = min(1024, walkPosition.x + walkWidth + radius);
            const auto top = max(0, walkPosition.y - radius);
            const auto bottom = min(1024, walkPosition.y + walkHeight + radius);

            // Pixel rectangle (make any even size units an extra WalkPosition)
            const auto topLeft = Position(position.x - unit.getType().dimensionLeft(), position.y - unit.getType().dimensionUp());
            const auto topRight = Position(position.x + unit.getType().dimensionRight() + 1, position.y - unit.getType().dimensionUp());
            const auto botLeft = Position(position.x - unit.getType().dimensionLeft(), position.y + unit.getType().dimensionDown() + 1);
            const auto botRight = Position(position.x + unit.getType().dimensionRight() + 1, position.y + unit.getType().dimensionDown() + 1);
            const auto x1 = position.x;
            const auto y1 = position.y;

            const auto clusterTopLeft = topLeft - Position(88, 88);
            const auto clusterBotRight = botRight + Position(88, 88);

            // If no nearby unit owned by self, ignore threat grids
            auto inRange = false;
            auto pState = unit.getPlayer() == Broodwar->self() ? PlayerState::Enemy : PlayerState::Self;
            auto closest = Util::getClosestUnit(position, pState, [&](auto&u) { return true; });
            if (closest) {
                const auto dist = Util::boxDistance(closest->getType(), closest->getPosition(), unit.getType(), position) - 128.0;
                const auto vision = max(closest->getType().sightRange(), unit.getType().sightRange());
                const auto range = max({ closest->getGroundRange(), closest->getAirRange(), unit.getGroundRange(), unit.getAirRange() });

                // If out of vision and range
                if (dist <= vision || dist <= range)
                    inRange = true;
            }

            // Iterate tiles and add to grid
            for (int x = left; x < right; x++) {
                for (int y = top; y < bottom; y++) {

                    // Cluster
                    if (clusterGrid && (unit.getPlayer() != Broodwar->self() || unit.getRole() == Role::Combat) && x * 8 >= clusterTopLeft.x && y * 8 >= clusterTopLeft.y && x * 8 <= clusterBotRight.x && y * 8 <= clusterBotRight.y) {
                        clusterGrid[x][y] += 1.0;
                        saveReset(x, y);
                    }

                    // Collision
                    if (!unit.isFlying() && !unit.isBurrowed() && Util::rectangleIntersect(topLeft, botRight, x * 8, y * 8) && Util::rectangleIntersect(topLeft, botRight, (x + 1) * 8 - 1, (y + 1) * 8 - 1)) {
                        collision[x][y] += 1;
                        saveReset(x, y);
                    }

                    // Threat
                    if (inRange && (grdGrid || airGrid)) {
                        const auto dist = fasterDistGrids(x1, y1, (x * 8) + 4, (y * 8) + 4);
                        if (grdGrid && dist <= unit.getGroundReach()) {
                            grdGrid[x][y] += float(unit.getVisibleGroundStrength()) / max(1.0f, log((float)dist));
                            saveReset(x, y);
                        }
                        if (airGrid && dist <= unit.getAirReach()) {
                            airGrid[x][y] += float(unit.getVisibleAirStrength()) / max(1.0f, log((float)dist));
                            saveReset(x, y);
                        }
                    }
                }
            }

            if (unit.getPlayer() != Broodwar->self())
                return;

            // Create remainder of collision pixels
            WalkPosition TL = WalkPosition(topLeft);
            WalkPosition TR = WalkPosition(topRight);
            WalkPosition BL = WalkPosition(botLeft);
            WalkPosition BR = WalkPosition(botRight);

            if (topLeft.y % 8 > 0) {
                for (auto x = TL.x; x < TR.x + 1; x++) {
                    verticalCollision[x][TL.y] = topLeft.y % 8;
                    saveReset(x, TL.y);
                }
            }
            if (botRight.y % 8 > 0) {
                for (auto x = BL.x; x < BR.x + 1; x++) {
                    verticalCollision[x][BL.y] = 8 - botRight.y % 8;
                    saveReset(x, BL.y);
                }
            }

            if (topLeft.x % 8 > 0) {
                for (auto y = TL.y; y < BL.y + 1; y++) {
                    horizontalCollision[TL.x][y] = topLeft.x % 8;
                    saveReset(TL.x, y);
                }
            }
            if (botRight.x % 8 > 0) {
                for (auto y = TR.y; y < BR.y + 1; y++) {
                    horizontalCollision[TR.x][y] = 8 - botRight.x % 8;
                    saveReset(TR.x, y);
                }
            }
        }

        void reset()
        {
            Visuals::startPerfTest();
            for (auto &w : resetVector) {
                int x = w.x;
                int y = w.y;

                aGroundCluster[x][y] = 0;
                aAirCluster[x][y] = 0;

                eGroundThreat[x][y] = 0.0;
                eAirThreat[x][y] = 0.0;
                eGroundCluster[x][y] = 0;
                eAirCluster[x][y] = 0;
                eSplash[x][y] = 0;

                collision[x][y] = 0;
                verticalCollision[x][y] = 0;
                horizontalCollision[x][y] = 0;
                resetGrid[x][y] = 0;
            }
            resetVector.clear();
            Visuals::endPerfTest("Grid Reset");
        }

        void updateAlly()
        {
            Visuals::startPerfTest();
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                UnitInfo &unit = *u;

                // Don't add these to grids
                if ((unit.unit()->exists() && (unit.unit()->isStasised() || unit.unit()->isMaelstrommed()))
                    || unit.getType() == Protoss_Interceptor
                    || unit.getType().isSpell()
                    || (unit.getPlayer() == Broodwar->self() && !unit.getType().isBuilding() && !unit.unit()->isCompleted()))
                    continue;

                // Pixel and walk sizes
                auto walkWidth = unit.getType().isBuilding() ? unit.getType().tileWidth() * 4 : (int)ceil(unit.getType().width() / 8.0);
                auto walkHeight = unit.getType().isBuilding() ? unit.getType().tileHeight() * 4 : (int)ceil(unit.getType().height() / 8.0);

                // Add a visited grid for rough guideline of what we've seen by this unit recently
                auto start = unit.getWalkPosition();
                for (int x = start.x - 2; x < start.x + walkWidth + 2; x++) {
                    for (int y = start.y - 2; y < start.y + walkHeight + 2; y++) {
                        auto t = WalkPosition(x, y);
                        if (t.isValid()) {
                            visitedGrid[x][y] = Broodwar->getFrameCount();
                            //Broodwar->drawBoxMap(Position(t), Position(t) + Position(9, 9), Colors::Green);
                        }
                    }
                }

                // Spider mines are added to the enemy splash grid so ally units avoid allied mines
                if (unit.getType() == Terran_Vulture_Spider_Mine) {
                    if (!unit.isBurrowed() && unit.hasTarget() && unit.getTarget().unit() && unit.getTarget().unit()->exists())
                        addSplash(unit);
                }

                else if (!unit.unit()->isLoaded())
                    addToGrids(unit, unit.getPosition(), unit.getWalkPosition());
            }
            Visuals::endPerfTest("Grid Self");
        }

        void updateEnemy()
        {
            Visuals::startPerfTest();
            for (auto &u : Units::getUnits(PlayerState::Enemy)) {
                UnitInfo &unit = *u;

                // Don't add these to grids
                if ((unit.unit()->exists() && (unit.unit()->isStasised() || unit.unit()->isMaelstrommed()))
                    || unit.getType() == Protoss_Interceptor
                    || (unit.getType().isWorker() && !unit.hasAttackedRecently())
                    || unit.getType().isSpell())
                    continue;

                if (unit.getType() == Terran_Vulture_Spider_Mine || unit.getType() == Protoss_Scarab) {
                    if (unit.hasTarget() && unit.getTarget().unit() && unit.getTarget().unit()->exists())
                        addSplash(unit);
                }
                else {
                    addToGrids(unit, unit.getPosition(), unit.getWalkPosition());
                }
            }
            Visuals::endPerfTest("Grid Enemy");
        }

        void updateNeutral()
        {
            // Collision Grid - TODO
            for (auto &u : Broodwar->neutral()->getUnits()) {

                WalkPosition start = WalkPosition(u->getTilePosition());
                int width = u->getType().tileWidth() * 4;
                int height = u->getType().tileHeight() * 4;
                if (u->getType().isFlyer()
                    || u->getType().isSpell())
                    continue;

                for (int x = start.x; x < start.x + width; x++) {
                    for (int y = start.y; y < start.y + height; y++) {

                        if (!WalkPosition(x, y).isValid())
                            continue;

                        collision[x][y] += 1;
                        saveReset(x, y);
                    }
                }
            }
        }

        void updateVisibility()
        {
            for (int x = 0; x < Broodwar->mapWidth(); x++) {
                for (int y = 0; y < Broodwar->mapHeight(); y++) {
                    TilePosition t(x, y);
                    if (Broodwar->isVisible(t))
                        visibleGrid[x][y] = Broodwar->getFrameCount();
                }
            }
        }

        void initializeMobility()
        {
            for (auto &gas : Broodwar->getGeysers()) {
                auto t = WalkPosition(gas->getTilePosition());
                for (int x = t.x; x < t.x + gas->getType().tileWidth() * 4; x++) {
                    for (int y = t.y; y < t.y + gas->getType().tileHeight() * 4; y++) {
                        mobility[x][y] = -1;
                    }
                }
            }

            for (int x = 0; x <= Broodwar->mapWidth() * 4; x++) {
                for (int y = 0; y <= Broodwar->mapHeight() * 4; y++) {

                    WalkPosition w(x, y);
                    if (!w.isValid() || mobility[x][y] != 0)
                        continue;

                    if (!Broodwar->isWalkable(w)) {
                        mobility[x][y] = -1;
                        continue;
                    }

                    for (int i = -12; i < 12; i++) {
                        for (int j = -12; j < 12; j++) {

                            WalkPosition w2(x + i, y + j);
                            if (w2.isValid() && mapBWEM.GetMiniTile(w2).Walkable() && mobility[x + i][y + j] != -1)
                                mobility[x][y] += 1;
                        }
                    }

                    mobility[x][y] = clamp(int(floor(mobility[x][y] / 56)), 1, 10);


                    // Island
                    if (mapBWEM.GetArea(w) && mapBWEM.GetArea(w)->AccessibleNeighbours().size() == 0)
                        mobility[x][y] = -1;
                }
            }
        }
    }

    void onFrame()
    {
        reset();
        updateVisibility();
        updateAlly();
        updateEnemy();
        updateNeutral();
    }

    void onStart()
    {
        initializeMobility();

        const auto areaBlocked = [&](const BWEM::Area * a) {
            if (!a)
                return false;
            for (auto &choke : a->ChokePoints()) {
                if (!choke->Blocked())
                    return false;
            }
            return true;
        };

        const auto fullyFogged = [&](auto &t) {
            for (int x = -2; x <= 2; x++) {
                for (int y = -2; y <= 2; y++) {
                    auto tile = t + TilePosition(x,y);
                    if (!tile.isValid())
                        continue;

                    if (BWEB::Map::isWalkable(tile))
                        return false;
                }
            }
            return true;
        };

        const auto fullyRanged = [&](auto &t) {
            for (int x = -6; x <= 6; x++) {
                for (int y = -6; y <= 6; y++) {
                    auto tile = t + TilePosition(x, y);
                    if (!tile.isValid())
                        continue;

                    if (BWEB::Map::isWalkable(tile) && Broodwar->getGroundHeight(tile) >= Broodwar->getGroundHeight(t) && !areaBlocked(mapBWEM.GetArea(tile)))
                        return false;
                }
            }
            return true;
        };

        if (Broodwar->getGameType() == GameTypes::Use_Map_Settings) {
            for (int x = 0; x < Broodwar->mapWidth() * 4; x++) {
                for (int y = 0; y < Broodwar->mapHeight() * 4; y++) {
                    visibleGrid[x][y] = -9999;
                }
            }
        }

        for (int x = 0; x < Broodwar->mapWidth(); x++) {
            for (int y = 0; y < Broodwar->mapHeight(); y++) {
                if (!fullyFogged(TilePosition(x, y)) || !fullyRanged(TilePosition(x,y)) || areaBlocked(mapBWEM.GetArea(TilePosition(x,y))))
                    cliffVision[x][y] = true;
                else
                    cliffVision[x][y] = false;
            }
        }
    }

    float getAGroundCluster(WalkPosition here) { return aGroundCluster[here.x][here.y]; }
    float getAGroundCluster(Position here) { return getAGroundCluster(WalkPosition(here)); }

    float getAAirCluster(WalkPosition here) { return aAirCluster[here.x][here.y]; }
    float getAAirCluster(Position here) { return getAAirCluster(WalkPosition(here)); }

    float getEGroundThreat(WalkPosition here) { return eGroundThreat[here.x][here.y]; }
    float getEGroundThreat(Position here) { return getEGroundThreat(WalkPosition(here)); }

    float getEAirThreat(WalkPosition here) { return eAirThreat[here.x][here.y]; }
    float getEAirThreat(Position here) { return getEAirThreat(WalkPosition(here)); }

    float getEGroundCluster(WalkPosition here) { return eGroundCluster[here.x][here.y]; }
    float getEGroundCluster(Position here) { return getEGroundCluster(WalkPosition(here)); }

    float getEAirCluster(WalkPosition here) { return eAirCluster[here.x][here.y]; }
    float getEAirCluster(Position here) { return getEAirCluster(WalkPosition(here)); }

    int getCollision(WalkPosition here) { return collision[here.x][here.y]; }
    int getVCollision(WalkPosition here) { return verticalCollision[here.x][here.y]; }
    int getHCollision(WalkPosition here) { return horizontalCollision[here.x][here.y]; }
    int getESplash(WalkPosition here) { return eSplash[here.x][here.y]; }

    int getMobility(WalkPosition here) { return mobility[here.x][here.y]; }
    int getMobility(Position here) { return getMobility(WalkPosition(here)); }

    int lastVisibleFrame(TilePosition t) { return visibleGrid[t.x][t.y]; }
    int lastVisitedFrame(WalkPosition w) { return visitedGrid[w.x][w.y]; }

    void addMovement(Position here, UnitInfo& unit)
    {
        if (unit.isLightAir()) {
            const auto walkWidth = unit.getWalkWidth();
            const auto walkHeight = unit.getWalkHeight();

            const auto left = max(0, WalkPosition(here).x - walkWidth - 4);
            const auto right = min(1024, WalkPosition(here).x + walkWidth + 4);
            const auto top = max(0, WalkPosition(here).y - walkHeight - 4);
            const auto bottom = min(1024, WalkPosition(here).y + walkHeight + 4);

            for (int x = left; x < right; x++) {
                for (int y = top; y < bottom; y++) {

                    //const auto dist = fasterDistGrids(x1, y1, (x * 8) + 4, (y * 8) + 4);
                    const auto dist = float(here.getDistance(Position((x * 8) + 4, (y * 8) + 4)));

                    //// Cluster
                    //if (dist < 64.0) {
                    //    aAirCluster[x][y] += (65.0f - dist) / 64.0f;
                    //    saveReset(x, y);
                    //}
                }
            }
        }
        else {
            addToGrids(unit, here, WalkPosition(here));
        }
    }

    bool hasCliffVision(TilePosition t) { return cliffVision[t.x][t.y]; }
}