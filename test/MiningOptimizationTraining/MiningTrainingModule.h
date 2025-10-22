#pragma once

#include "Modules/InstrumentedDoNothingModule.h"

class MiningTrainingModule : public InstrumentedDoNothingModule
{
public:
    MiningTrainingModule() : InstrumentedDoNothingModule(false) {}

    void onStart() override;

    void onFrame() override;
};
