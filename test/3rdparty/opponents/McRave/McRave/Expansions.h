#pragma once
#include "McRave.h"

namespace McRave::Expansion {

    void onFrame();
    void onStart();

    bool expansionBlockersExists();
    std::vector<BWEB::Station*> getExpandOrder();
}
