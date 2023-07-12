#pragma once
#include <BWAPI.h>
#include <sstream>
#include <set>

namespace McRave::BuildOrder {

    // Need a namespace to share variables among the various files used
    namespace All {
        inline std::map <BWAPI::UnitType, int> buildQueue;
        inline bool inOpeningBook = true;
        inline bool getTech = false;
        inline bool wallNat = false;
        inline bool wallMain = false;
        inline bool scout = false;
        inline bool productionSat = false;
        inline bool techSat = false;
        inline bool wantNatural = false;
        inline bool wantThird = false;
        inline bool proxy = false;
        inline bool hideTech = false;
        inline bool playPassive = false;
        inline bool rush = false;
        inline bool pressure = false;
        inline bool lockedTransition = false;
        inline bool gasTrick = false;
        inline bool inBookSupply = false;
        inline bool transitionReady = false;
        inline bool planEarly = false;

        inline bool gasDesired = false;
        inline bool expandDesired = false;
        inline bool rampDesired = false;

        inline std::map<BWAPI::UnitType, int> unitLimits;
        inline std::map<BWAPI::UnitType, int> unitReservations;
        inline int gasLimit = INT_MAX;
        inline int s = 0;

        inline std::string currentBuild = "";
        inline std::string currentOpener = "";
        inline std::string currentTransition = "";

        inline BWAPI::UpgradeType firstUpgrade = BWAPI::UpgradeTypes::None;
        inline BWAPI::TechType firstTech = BWAPI::TechTypes::None;
        inline BWAPI::UnitType firstUnit = BWAPI::UnitTypes::None;
        inline BWAPI::UnitType techUnit = BWAPI::UnitTypes::None;
        inline BWAPI::UnitType desiredDetection = BWAPI::UnitTypes::None;
        inline std::vector<BWAPI::UnitType> techOrder;
        inline std::set <BWAPI::UnitType> techList;
        inline std::set <BWAPI::UnitType> unlockedType;

        inline std::map <BWAPI::UnitType, double> armyComposition;
    }

    double getCompositionPercentage(BWAPI::UnitType);
    std::map<BWAPI::UnitType, double> getArmyComposition();
    int buildCount(BWAPI::UnitType);
    bool firstReady();
    bool unlockReady(BWAPI::UnitType);

    void onFrame();
    void getNewTech();
    void getTechBuildings();

    int getGasQueued();
    int getMinQueued();
    bool shouldRamp();
    bool shouldExpand();
    bool techComplete();
    bool atPercent(BWAPI::UnitType, double);
    bool atPercent(BWAPI::TechType, double);

    std::map<BWAPI::UnitType, int>& getBuildQueue();
    BWAPI::UnitType getTechUnit();
    BWAPI::UpgradeType getFirstUpgrade();
    BWAPI::TechType getFirstTech();
    std::set <BWAPI::UnitType>& getTechList();
    std::set <BWAPI::UnitType>& getUnlockedList();
    int gasWorkerLimit();
    int getUnitReservation(BWAPI::UnitType);
    bool isUnitUnlocked(BWAPI::UnitType);
    bool isTechUnit(BWAPI::UnitType);
    bool isOpener();
    bool takeNatural();
    bool takeThird();
    bool shouldScout();
    bool isWallNat();
    bool isWallMain();
    bool isProxy();
    bool isHideTech();
    bool isPlayPassive();
    bool isRush();
    bool isPressure();
    bool isGasTrick();
    bool isPlanEarly();
    std::string getCurrentBuild();
    std::string getCurrentOpener();
    std::string getCurrentTransition();

    void setLearnedBuild(std::string, std::string, std::string);
}