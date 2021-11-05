#pragma once
#include <BWAPI.h>

using namespace BWAPI;

namespace McRave::Util {

    const BWEM::ChokePoint * getClosestChokepoint(BWAPI::Position);

    double getCastRadius(BWAPI::TechType);
    double getCastLimit(BWAPI::TechType);

    int boxDistance(BWAPI::UnitType, BWAPI::Position, BWAPI::UnitType, BWAPI::Position);

    bool rectangleIntersect(BWAPI::Position, BWAPI::Position, BWAPI::Position);
    bool rectangleIntersect(BWAPI::Position, BWAPI::Position, int, int);
    bool findWalkable(UnitInfo&, BWAPI::Position&, bool visual = false);

    BWAPI::Position clipLine(BWAPI::Position, BWAPI::Position);
    BWAPI::Position clipPosition(BWAPI::Position);
    BWAPI::Position projectLine(std::pair<BWAPI::Position, BWAPI::Position>, BWAPI::Position);

    Time getTime();

    void onStart();
    void onFrame();

    template<typename F>
    bool isBetweenC(F a, F b, F c) {
        return a >= b && a <= c;
    }

    template<typename F>
    bool isBetweenX(F a, F b, F c) {
        return a > b && a < c;
    }

    std::shared_ptr<UnitInfo> getClosestUnit(BWAPI::Position here, PlayerState player, std::function<bool(UnitInfo&)> &&pred);

    std::shared_ptr<UnitInfo> getFurthestUnit(BWAPI::Position here, PlayerState player, std::function<bool(UnitInfo&)> &&pred);

    std::shared_ptr<UnitInfo> getClosestUnitGround(BWAPI::Position here, PlayerState player, std::function<bool(UnitInfo&)> &&pred);

    std::shared_ptr<UnitInfo> getFurthestUnitGround(BWAPI::Position here, PlayerState player, std::function<bool(UnitInfo&)> &&pred);

    // Written by Hannes 8)
    template<typename T, int idx = 0>
    int iterateCommands(T const &tpl, UnitInfo& unit) {
        if constexpr (idx < std::tuple_size<T>::value)
            if (!std::get<idx>(tpl)(unit))
                return iterateCommands<T, idx + 1>(tpl, unit);
        return idx;
    }

    void testPointOnPath(BWEB::Path& path, std::function<bool(Position)> &&pred);

    void testAllPointOnPath(BWEB::Path& path, std::function<bool(Position)> &&pred);

    BWAPI::Position findPointOnPath(BWEB::Path& path, std::function<bool(Position)> &&pred, int cnt = INT_MAX);

    std::vector<BWAPI::Position> findAllPointOnPath(BWEB::Path& path, std::function<bool(Position)> &&pred);
}
