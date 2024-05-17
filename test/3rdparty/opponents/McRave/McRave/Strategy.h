#pragma once
#include <BWAPI.h>

namespace McRave::Strategy {

    std::string getEnemyBuild();
    std::string getEnemyOpener();
    std::string getEnemyTransition();
    Time getEnemyBuildTime();
    Time getEnemyOpenerTime();
    Time getEnemyTransitionTime();

    int getWorkersNearUs();
    bool enemyFastExpand();
    bool enemyRush();
    bool enemyInvis();
    bool enemyPossibleProxy();
    bool enemyProxy();
    bool enemyGasSteal();
    bool enemyBust();
    bool enemyPressure();
    bool enemyWalled();
    bool enemyGreedy();

    void onFrame();
}
