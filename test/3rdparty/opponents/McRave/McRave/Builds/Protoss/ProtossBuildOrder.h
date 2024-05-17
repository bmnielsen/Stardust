#pragma once

namespace McRave::BuildOrder::Protoss {
    void opener();
    void tech();
    void situational();
    void composition();
    void unlocks();

    void PvP_1GC();
    void PvP_2G();

    void PvT_1GC();
    void PvT_2G();
    void PvT_2B();

    void PvZ_1GC();
    void PvZ_2G();
    void PvZ_FFE();

    void PvFFA_1GC();

    bool goonRange();
    bool enemyMoreZealots();
    bool enemyMaybeDT();
    void defaultPvP();
    void defaultPvT();
    void defaultPvZ();

    void PvT();
    void PvP();
    void PvZ();
    void PvFFA();
}