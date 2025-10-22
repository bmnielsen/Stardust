#pragma once

#include "MiningOptimizationTraining/MiningTrainingModule.h"

class FullSaturationPathExplorationModule : public MiningTrainingModule
{
public:
    explicit FullSaturationPathExplorationModule(unsigned int cannons)
            : MiningTrainingModule()
            , cannons(cannons)
    {}

    void onFrame() override;

private:
    unsigned int cannons;

    std::map<BWAPI::Position, std::pair<int, Base*>> workerCreationOrderAndBase;
    std::map<BWAPI::Unit, Resource> workerToAssignedPatch;

    bool initialize();
};
