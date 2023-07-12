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

        // Mobility Grid
        int mobility[1024][1024] ={};
        int collision[1024][1024] ={};
        int horizontalCollision[1024][1024] ={};
        int verticalCollision[1024][1024] ={};

        bool cliffVision[256][256] ={};

        bool canAddToGrid(UnitInfo& unit)
        {
            // Don't add these to grids
            if ((unit.unit()->exists() && (unit.unit()->isStasised() || unit.unit()->isMaelstrommed() || unit.unit()->isLoaded()))
                || !unit.getPosition().isValid()
                || unit.getType() == Protoss_Interceptor
                || unit.getType().isSpell()
                || unit.getType() == Terran_Vulture_Spider_Mine
                || unit.getType() == Protoss_Scarab
                || (unit.getPlayer() == Broodwar->self() && !unit.getType().isBuilding() && !unit.unit()->isCompleted())
                || (unit.getPlayer() != Broodwar->self() && unit.getType().isWorker() && !unit.hasAttackedRecently() && !Scouts::gatheringInformation())
                || (unit.getPlayer() != Broodwar->self() && !unit.hasTarget() && !unit.unit()->exists() && !Scouts::gatheringInformation()))
                return false;
            return true;
        }

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

        void addToGrids(UnitInfo& unit, bool collisionReq, bool threatReq)
        {
            // Choose threat grid
            auto grdGrid = (unit.getPlayer() == Broodwar->self() || !unit.canAttackGround()) ? nullptr : eGroundThreat;
            auto airGrid = (unit.getPlayer() == Broodwar->self() || !unit.canAttackAir()) ? nullptr : eAirThreat;

            // Choose cluster grid
            auto clusterGrid = unit.getPlayer() == Broodwar->self() ?
                (unit.getType().isFlyer() ? aAirCluster : aGroundCluster) :
                (unit.getType().isFlyer() ? eAirCluster : eGroundCluster);

            // Limit checks so we don't have to check validity
            auto radius = unit.getPlayer() != Broodwar->self() ? 1 + int(max(unit.getGroundReach(), unit.getAirReach())) / 8 : 8;

            if (unit.getType().isWorker() &&
                (unit.unit()->isConstructing() || unit.unit()->isGatheringGas() || unit.unit()->isGatheringMinerals()))
                radius = radius / 3;

            const auto left = max(0, unit.getWalkPosition().x - radius);
            const auto right = min(1024, unit.getWalkPosition().x + unit.getWalkWidth() + radius);
            const auto top = max(0, unit.getWalkPosition().y - radius);
            const auto bottom = min(1024, unit.getWalkPosition().y + unit.getWalkHeight() + radius);

            // Pixel rectangle (make any even size units an extra WalkPosition)
            const auto topLeft = Position(unit.getPosition().x - unit.getType().dimensionLeft(), unit.getPosition().y - unit.getType().dimensionUp());
            const auto topRight = Position(unit.getPosition().x + unit.getType().dimensionRight() + 1, unit.getPosition().y - unit.getType().dimensionUp());
            const auto botLeft = Position(unit.getPosition().x - unit.getType().dimensionLeft(), unit.getPosition().y + unit.getType().dimensionDown() + 1);
            const auto botRight = Position(unit.getPosition().x + unit.getType().dimensionRight() + 1, unit.getPosition().y + unit.getType().dimensionDown() + 1);
            const auto x1 = unit.getPosition().x;
            const auto y1 = unit.getPosition().y;

            const auto clusterTopLeft = topLeft - Position(88, 88);
            const auto clusterBotRight = botRight + Position(88, 88);

            // Create remainder of collision pixels
            WalkPosition TL = WalkPosition(topLeft);
            WalkPosition TR = WalkPosition(topRight);
            WalkPosition BL = WalkPosition(botLeft);
            WalkPosition BR = WalkPosition(botRight);

            if (!unit.isFlying()) {
                if (topLeft.y % 8 > 0) {
                    for (auto x = TL.x; x <= TR.x; x++) {
                        verticalCollision[x][TL.y] = topLeft.y % 8;
                        //Broodwar->drawTextMap(Position(WalkPosition(x, TL.y)), "%d", verticalCollision[x][TL.y]);
                        saveReset(x, TL.y);
                    }
                }
                if (botRight.y % 8 > 0) {
                    for (auto x = BL.x; x <= BR.x; x++) {
                        verticalCollision[x][BL.y] = 8 - botRight.y % 8;
                        //Broodwar->drawTextMap(Position(WalkPosition(x, BL.y)), "%d", verticalCollision[x][BL.y]);
                        saveReset(x, BL.y);
                    }
                }

                if (topLeft.x % 8 > 0) {
                    for (auto y = TL.y; y <= BL.y; y++) {
                        horizontalCollision[TL.x][y] = topLeft.x % 8;
                        //Broodwar->drawTextMap(Position(WalkPosition(TL.x, y)), "%d", horizontalCollision[TL.x][y]);
                        saveReset(TL.x, y);
                    }
                }
                if (botRight.x % 8 > 0) {
                    for (auto y = TR.y; y <= BR.y; y++) {
                        horizontalCollision[TR.x][y] = 8 - botRight.x % 8;
                        //Broodwar->drawTextMap(Position(WalkPosition(TR.x, y)), "%d", horizontalCollision[TR.x][y]);
                        saveReset(TR.x, y);
                    }
                }
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
                    if (grdGrid || airGrid) {
                        const auto dist = fasterDistGrids(x1, y1, (x * 8) + 4, (y * 8) + 4);
                        if (grdGrid && dist <= unit.getGroundReach()) {
                            grdGrid[x][y] += float(unit.getVisibleGroundStrength() / max(1.0, logLookup16[dist/16]));
                            saveReset(x, y);
                        }
                        if (airGrid && dist <= unit.getAirReach()) {
                            airGrid[x][y] += float(unit.getVisibleAirStrength() / max(1.0, logLookup16[dist/16]));
                            saveReset(x, y);
                        }
                    }
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
                if (!canAddToGrid(unit))
                    continue;

                // Add a visited grid for rough guideline of what we've seen by this unit recently
                auto start = unit.getWalkPosition();
                for (int x = start.x - 2; x < start.x + unit.getWalkWidth() + 2; x++) {
                    for (int y = start.y - 2; y < start.y + unit.getWalkHeight() + 2; y++) {
                        auto t = WalkPosition(x, y);
                        visitedGrid[x][y] = Broodwar->getFrameCount();
                    }
                }
                addToGrids(unit, false, false);
            }
            Visuals::endPerfTest("Grid Self");
        }

        void updateEnemy()
        {
            Visuals::startPerfTest();
            for (auto &u : Units::getUnits(PlayerState::Enemy)) {
                UnitInfo &unit = *u;

                // Don't add these to grids
                if (!canAddToGrid(unit))
                    continue;

                //// Prevent running grids if the unit is irrelevant
                //if (!unit.getType().isBuilding() && unit.hasTarget()) {
                //    auto unitTarget = unit.getTarget().lock();
                //    auto maxRange = max({ unit.getAirRange(), unit.getGroundRange(), double(unit.getType().sightRange()),
                //        unitTarget->getAirRange(), unitTarget->getGroundRange(), double(unitTarget->getType().sightRange()),
                //        unitTarget->getEngageRadius(), unitTarget->getRetreatRadius() }) + 160.0;
                //    if (unit.getPosition().getDistance(unitTarget->getPosition()) > maxRange)
                //        continue;
                //}

                addToGrids(unit, !unit.isFlying(), true);
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

        void initializeLogTable()
        {
            for (int i = 0; i < 1024; i++) {
                logLookup16[i] = log(16 * i);
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
        initializeLogTable();
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
                    auto tile = t + TilePosition(x, y);
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
                if (!fullyFogged(TilePosition(x, y)) || !fullyRanged(TilePosition(x, y)) || areaBlocked(mapBWEM.GetArea(TilePosition(x, y))))
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

    int getMobility(WalkPosition here) { return mobility[here.x][here.y]; }
    int getMobility(Position here) { return getMobility(WalkPosition(here)); }

    int lastVisibleFrame(TilePosition t) { return visibleGrid[t.x][t.y]; }
    int lastVisitedFrame(WalkPosition w) { return visitedGrid[w.x][w.y]; }

    bool hasCliffVision(TilePosition t) { return cliffVision[t.x][t.y]; }
}