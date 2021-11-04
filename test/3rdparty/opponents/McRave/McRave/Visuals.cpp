#include "McRave.h"
#include <chrono>

using namespace BWAPI;
using namespace std;

namespace McRave::Visuals {

    namespace {
        chrono::steady_clock::time_point start;
        int screenOffset = 0;

        struct FrameTest {
            double current = 0.0;
            double average = 0.0;
            double maximum = 0.0;
            string name = "";

            FrameTest(string _name) {
                name = _name;
            }

            void update(double dur) {
                average = average * 0.99 + dur * 0.01;
                current = dur;
                maximum = max(maximum, dur);
            }
        };
        vector<FrameTest> frameTests;

        bool targets = false;
        bool builds = true;
        bool bweb = false;
        bool sim = false;
        bool paths = false;
        bool strengths = false;
        bool orders = false;
        bool local = false;
        bool global = false;
        bool resources = false;
        bool timers = true;
        bool scores = true;
        bool roles = false;

        void drawInformation()
        {
            // BWAPIs green text doesn't point to the right color so this will be seen occasionally
            int color = Broodwar->self()->getColor();
            int textColor = color == 185 ? textColor = Text::DarkGreen : Broodwar->self()->getTextColor();

            // Reset the screenOffset for the performance tests
            screenOffset = 0;

            // Builds
            if (builds) {
                Broodwar->drawTextScreen(432, 16, "%c%s: %c%s %s", Text::White, BuildOrder::getCurrentBuild().c_str(), Text::Grey, BuildOrder::getCurrentOpener().c_str(), BuildOrder::getCurrentTransition().c_str());
                Broodwar->drawTextScreen(432, 26, "%c%s: %c%s %s", Text::White, Strategy::getEnemyBuild().c_str(), Text::Grey, Strategy::getEnemyOpener().c_str(), Strategy::getEnemyTransition().c_str());
                Broodwar->drawTextScreen(160, 0, "%cExpanding", BuildOrder::shouldExpand() ? Text::White : Text::Grey);
                Broodwar->drawTextScreen(160, 10, "%cRamping", BuildOrder::shouldRamp() ? Text::White : Text::Grey);
                Broodwar->drawTextScreen(160, 20, "%cTeching", BuildOrder::getTechUnit() != UnitTypes::None ? Text::White : Text::Grey);
            }

            // Scores
            if (scores) {
                int offset = 0;
                for (auto &type : UnitTypes::allUnitTypes()) {
                    auto scoreColor = BuildOrder::isTechUnit(type) ? Text::White : Text::Grey;
                    if ((total(type) > 0 && !type.isBuilding()) || BuildOrder::getCompositionPercentage(type) > 0.00) {
                        Broodwar->drawTextScreen(0, offset, "%c%s  %d/%d  %.2f", scoreColor, type.c_str(), vis(type), total(type), BuildOrder::getCompositionPercentage(type));
                        offset += 10;
                    }
                }
                Broodwar->drawTextScreen(0, offset, "%c%s", Text::Grey, BuildOrder::getTechUnit().c_str());
            }

            // Timers
            if (timers) {
                int time = Broodwar->getFrameCount() / 24;
                int seconds = time % 60;
                int minute = time / 60;
                Broodwar->drawTextScreen(432, 36, "%c%d", Text::Grey, Broodwar->getFrameCount());
                Broodwar->drawTextScreen(482, 36, "%c%d:%02d", Text::Grey, minute, seconds);

                sort(frameTests.begin(), frameTests.end(), [&](auto &left, auto &right) { return left.average > right.average; });

                auto overall = 0.0;
                for (auto &test : frameTests) {
                    if (test.average > 0.25 || test.current > 10.0 || test.maximum > 25.0) {
                        Broodwar->drawTextScreen(230, screenOffset, "%c%s", Text::White, test.name.c_str());
                        Broodwar->drawTextScreen(322, screenOffset, "%c%.1fms, %.1fms, %.1fms", Text::White, test.average, test.current, test.maximum);
                        screenOffset += 10;
                    }
                    overall += test.current;
                }

                if (overall > 10000) {
                    Broodwar << "10s DQ at: " << Util::getTime() << endl;
                    McRave::easyWrite("10s DQ at: " + Util::getTime().toString());
                }
                else if (overall > 1000) {
                    Broodwar << "1s DQ at: " << Util::getTime() << endl;
                    McRave::easyWrite("1s DQ at: " + Util::getTime().toString());
                }
                else if (overall > 55) {
                    Broodwar << "55ms DQ at: " << Util::getTime() << endl;
                    McRave::easyWrite("55ms DQ at: " + Util::getTime().toString());
                }
            }

            // Resource
            if (resources) {
                for (auto &r : Resources::getMyMinerals()) {
                    auto &resource = *r;
                    Broodwar->drawTextMap(resource.getPosition() + Position(-8, 8), "%c%d", Text::GreyBlue, (*r).getRemainingResources());
                    Broodwar->drawTextMap(resource.getPosition() - Position(32, 8), "%cGatherers: %d", textColor, resource.getGathererCount());
                    Broodwar->drawTextMap(resource.getPosition() - Position(32, 16), "%cState: %d", textColor, (int)resource.getResourceState());

                }
                for (auto &r : Resources::getMyGas()) {
                    auto &resource = *r;
                    Broodwar->drawTextMap(resource.getPosition() + Position(-8, 8), "%c%d", Text::Green, (*r).getRemainingResources());
                    Broodwar->drawTextMap(resource.getPosition() - Position(32, 8), "%cGatherers: %d", textColor, resource.getGathererCount());
                    Broodwar->drawTextMap(resource.getPosition() - Position(32, 16), "%cState: %d", textColor, (int)resource.getResourceState());
                }

                Broodwar->drawTextScreen(Position(4, 64), "%cReserved: %d", Text::GreyBlue, Production::getReservedMineral());
                Broodwar->drawTextScreen(Position(4, 80), "%cReserved: %d", Text::Green, Production::getReservedGas());
                Broodwar->drawTextScreen(Position(4, 96), "%cPlanned: %d", Text::GreyBlue, Planning::getPlannedMineral());
                Broodwar->drawTextScreen(Position(4, 112), "%cPlanned: %d", Text::Green, Planning::getPlannedGas());
            }


            // BWEB
            if (bweb) {
                BWEB::Map::draw();
            }
        }

        void drawAllyInfo()
        {
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                UnitInfo &unit = *u;

                int color = unit.getPlayer()->getColor();
                int textColor = color == 185 ? textColor = Text::DarkGreen : unit.getPlayer()->getTextColor();

                if (unit.unit()->isLoaded())
                    continue;

                if (targets) {
                    if (unit.hasTarget())
                        Visuals::drawLine(unit.getTarget().getPosition(), unit.getPosition(), color);
                }

                if (resources) {
                    if (unit.hasResource())
                        Visuals::drawLine(unit.getResource().getPosition(), unit.getPosition(), color);
                }

                if (strengths) {
                    if (unit.getVisibleGroundStrength() > 0.0 || unit.getVisibleAirStrength() > 0.0) {
                        Broodwar->drawTextMap(unit.getPosition() + Position(5, -10), "Grd: %c %.2f", Text::Brown, unit.getVisibleGroundStrength());
                        Broodwar->drawTextMap(unit.getPosition() + Position(5, 2), "Air: %c %.2f", Text::Blue, unit.getVisibleAirStrength());
                    }
                }

                if (orders) {
                    int width = unit.getType().isBuilding() ? -16 : unit.getType().width() / 2;
                    if (unit.getRole() == Role::Production && (unit.getType() == UnitTypes::Zerg_Egg || (unit.unit()->isTraining() && !unit.unit()->getTrainingQueue().empty()))) {
                        auto trainType = unit.getType() == UnitTypes::Zerg_Egg ? unit.unit()->getBuildType().c_str() : unit.unit()->getTrainingQueue().front().c_str();
                        auto trainOrder =  unit.getType() == UnitTypes::Zerg_Egg ? "Morphing" : "Training";
                        Broodwar->drawTextMap(unit.getPosition() + Position(width, -8), "%c%s", textColor, trainOrder);
                        Broodwar->drawTextMap(unit.getPosition() + Position(width, 0), "%c%s", textColor, trainType);
                    }
                    else if (unit.unit() && unit.unit()->exists() && unit.unit()->isCompleted()) {
                        if (unit.unit()->getOrder() != Orders::Nothing)
                            Broodwar->drawTextMap(unit.getPosition() + Position(width, -8), "%c%s", textColor, unit.unit()->getOrder().c_str());
                        if (unit.unit()->isUpgrading())
                            Broodwar->drawTextMap(unit.getPosition() + Position(width, 0), "%c%s", textColor, unit.unit()->getUpgrade().c_str());
                        else if (unit.unit()->isResearching())
                            Broodwar->drawTextMap(unit.getPosition() + Position(width, 0), "%c%s", textColor, unit.unit()->getTech().c_str());
                    }
                }

                if (local) {
                    if (unit.getRole() == Role::Combat)
                        Broodwar->drawTextMap(unit.getPosition(), "%c%d", textColor, unit.getLocalState());
                }
                if (global) {
                    if (unit.getRole() == Role::Combat)
                        Broodwar->drawTextMap(unit.getPosition(), "%c%d", textColor, unit.getGlobalState());
                }

                if (sim) {
                    if (unit.getRole() == Role::Combat) {
                        int width = unit.getType().isBuilding() ? -16 : unit.getType().width() / 2;
                        Broodwar->drawTextMap(unit.getPosition() + Position(width, 8), "%c%.2f", Text::White, unit.getSimValue());
                    }
                }

                if (roles) {
                    int width = unit.getType().isBuilding() ? -16 : unit.getType().width() / 2;
                    Broodwar->drawTextMap(unit.getPosition() + Position(width, 16), "%c%d", Text::White, unit.getRole());
                }
            }
        }

        void drawEnemyInfo()
        {
            for (auto &u : Units::getUnits(PlayerState::Enemy)) {
                UnitInfo &unit = *u;

                int color = unit.getPlayer()->getColor();
                int textColor = color == 185 ? textColor = Text::DarkGreen : unit.getPlayer()->getTextColor();

                if (targets) {
                    if (unit.hasTarget())
                        Visuals::drawLine(unit.getTarget().getPosition(), unit.getPosition(), color);
                }

                if (strengths) {
                    if (unit.getVisibleGroundStrength() > 0.0 || unit.getVisibleAirStrength() > 0.0) {
                        Broodwar->drawTextMap(unit.getPosition() + Position(5, -10), "Grd: %c %.2f", Text::Brown, unit.getVisibleGroundStrength());
                        Broodwar->drawTextMap(unit.getPosition() + Position(5, 2), "Air: %c %.2f", Text::Blue, unit.getVisibleAirStrength());
                    }
                }

                if (orders) {
                    if (unit.unit() && unit.unit()->exists())
                        Broodwar->drawTextMap(unit.getPosition(), "%c%s", textColor, unit.unit()->getOrder().c_str());
                }
            }
        }
    }

    void centerCameraOn(Position here)
    {
        Broodwar->setScreenPosition(here - Position(320, 180));
    }

    void onFrame()
    {
        drawInformation();
        drawAllyInfo();
        drawEnemyInfo();
    }

    void endPerfTest(string function)
    {
        double dur = std::chrono::duration <double, std::milli>(std::chrono::high_resolution_clock::now() - start).count();
        bool found = false;
        for (auto &test : frameTests) {
            if (test.name == function) {
                test.update(dur);
                found = true;
            }
        }

        if (!found)
            frameTests.push_back(FrameTest(function));
    }

    void startPerfTest()
    {
        start = chrono::high_resolution_clock::now();
    }

    void onSendText(string text)
    {
        if (text == "/targets")              targets = !targets;
        else if (text == "/sim")             sim = !sim;
        else if (text == "/strengths")       strengths = !strengths;
        else if (text == "/builds")          builds = !builds;
        else if (text == "/bweb")            bweb = !bweb;
        else if (text == "/paths")           paths = !paths;
        else if (text == "/orders")          orders = !orders;
        else if (text == "/local")           local = !local;
        else if (text == "/global")          global = !global;
        else if (text == "/resources")       resources = !resources;
        else if (text == "/timers")          timers = !timers;
        else if (text == "/roles")           roles = !roles;
        else                                 Broodwar->sendText("%s", text.c_str());
        return;
    }

    void drawPath(BWEB::Path& path)
    {
        int color = Broodwar->self()->getColor();
        if (paths && !path.getTiles().empty()) {
            TilePosition next = path.getSource();
            for (auto &tile : path.getTiles()) {
                if (next.isValid() && tile.isValid()) {
                    Visuals::drawLine(Position(next) + Position(16, 16), Position(tile) + Position(16, 16), color);
                    Visuals::drawCircle(Position(next) + Position(16, 16), 4, color, true);
                }

                next = tile;
            }
        }
    }

    void drawDebugText(std::string s, double d) {
        Broodwar->drawTextScreen(Position(0, 50 + screenOffset), "%s: %.2f", s.c_str(), d);
        screenOffset += 10;
    }

    void drawDebugText(std::string s, int i) {
        Broodwar->drawTextScreen(Position(0, 50 + screenOffset), "%s: %d", s.c_str(), i);
        screenOffset += 10;
    }

    void drawBox(Position here, Position there, Color color, bool solid) {
        here = Util::clipPosition(here);
        there = Util::clipPosition(there);
        Broodwar->drawBoxMap(here, there, color, solid);
    }
    void drawBox(WalkPosition here, WalkPosition there, Color color, bool solid) {
        drawBox(Position(here), Position(there), color, solid);
    }
    void drawBox(TilePosition here, TilePosition there, Color color, bool solid) {
        drawBox(Position(here), Position(there), color, solid);
    }

    void drawCircle(Position here, int radius, Color color, bool solid) {
        here = Util::clipPosition(here);
        Broodwar->drawCircleMap(here, radius, color, solid);
    }
    void drawCircle(WalkPosition here, int radius, Color color, bool solid) {
        drawCircle(Position(here), radius, color, solid);
    }
    void drawCircle(TilePosition here, int radius, Color color, bool solid) {
        drawCircle(Position(here), radius, color, solid);
    }

    void drawLine(const BWEM::ChokePoint * choke, Color color)
    {
        Broodwar->drawLineMap(Position(choke->Pos(choke->end1)), Position(choke->Pos(choke->end2)), color);
    }
    void drawLine(Position here, Position there, Color color) {
        here = Util::clipPosition(here);
        there = Util::clipPosition(there);
        Broodwar->drawLineMap(here, there, color);
    }
    void drawLine(WalkPosition here, WalkPosition there, Color color) {
        drawLine(Position(here), Position(there), color);
    }
    void drawLine(TilePosition here, TilePosition there, Color color) {
        drawLine(Position(here), Position(there), color);
    }
}