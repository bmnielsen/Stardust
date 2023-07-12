#pragma once
#include <BWAPI.h>

namespace McRave::Stations
{
    std::multimap<double, BWEB::Station *>& getStationsBySaturation();
    std::multimap<double, BWEB::Station *>& getStationsByProduction();

    void onFrame();
    void onStart();
    void storeStation(BWAPI::Unit);
    void removeStation(BWAPI::Unit);
    int needGroundDefenses(BWEB::Station*);
    int needAirDefenses(BWEB::Station*);
    int getGroundDefenseCount(BWEB::Station*);
    int getAirDefenseCount(BWEB::Station*);
    bool needPower(BWEB::Station*);
    bool isIsland(BWEB::Station*);
    bool isBaseExplored(BWEB::Station*);
    bool isGeyserExplored(BWEB::Station*);
    bool isCompleted(BWEB::Station*);
    int lastVisible(BWEB::Station*);
    double getSaturationRatio(BWEB::Station *);
    BWAPI::Position getDefendPosition(BWEB::Station *);
    BWEB::Station * getClosestRetreatStation(UnitInfo&);
    int getGasingStationsCount();
    int getMiningStationsCount();
    int getMineralsRemaining(BWEB::Station *);
    int getGasRemaining(BWEB::Station *);
    int getMineralsInitial(BWEB::Station *);
    int getGasInitial(BWEB::Station *);

    PlayerState ownedBy(BWEB::Station *);

    std::vector<BWEB::Station*> getStations(PlayerState);
    template<typename F>
    std::vector<BWEB::Station*> getStations(PlayerState, F &&pred);

    template<typename F>
    BWEB::Station* getClosestStationAir(BWAPI::Position here, PlayerState player, F &&pred) {
        auto &list = getStations(player);
        auto distBest = DBL_MAX;
        BWEB::Station * closestStation = nullptr;
        for (auto &station : list) {
            double dist = here.getDistance(station->getBase()->Center());
            if (dist < distBest && pred(station)) {
                closestStation = station;
                distBest = dist;
            }
        }
        return closestStation;
    }

    inline BWEB::Station* getClosestStationAir(BWAPI::Position here, PlayerState player) {
        return getClosestStationAir(here, player, [](auto) {
            return true;
        });
    }

    template<typename F>
    BWEB::Station* getClosestStationGround(BWAPI::Position here, PlayerState player, F &&pred) {
        auto &list = getStations(player);
        auto distBest = DBL_MAX;
        BWEB::Station * closestStation = nullptr;
        for (auto &station : list) {
            double dist = BWEB::Map::getGroundDistance(here, station->getBase()->Center());
            if (dist < distBest && pred(station)) {
                closestStation = station;
                distBest = dist;
            }
        }
        return closestStation;
    }

    inline BWEB::Station* getClosestStationGround(BWAPI::Position here, PlayerState player) {
        return getClosestStationGround(here, player, [](auto) {
            return true;
        });
    }
};
