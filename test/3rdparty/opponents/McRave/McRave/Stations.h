#pragma once
#include <BWAPI.h>

namespace McRave::Stations
{
    std::vector<BWEB::Station*>& getMyStations();
    std::vector<BWEB::Station*>& getEnemyStations();
    std::multimap<double, BWEB::Station *>& getStationsBySaturation();
    BWEB::Station * getClosestStationAir(PlayerState, BWAPI::Position);
    BWEB::Station * getClosestStationGround(PlayerState, BWAPI::Position);

    void onFrame();
    void onStart();
    void storeStation(BWAPI::Unit);
    void removeStation(BWAPI::Unit);
    int needGroundDefenses(BWEB::Station*);
    int needAirDefenses(BWEB::Station*);
    int getGroundDefenseCount(BWEB::Station*);
    int getAirDefenseCount(BWEB::Station*);
    bool needPower(BWEB::Station*);
    bool isBaseExplored(BWEB::Station*);
    bool isGeyserExplored(BWEB::Station*);
    bool isCompleted(BWEB::Station*);
    int lastVisible(BWEB::Station*);
    double getSaturationRatio(BWEB::Station *);
    BWAPI::Position getDefendPosition(BWEB::Station *);

    PlayerState ownedBy(BWEB::Station *);
};
