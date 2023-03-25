#pragma once

#include "MainArmyPlay.h"
#include "Squads/DefendWallSquad.h"
#include "Squads/WorkerDefenseSquad.h"

class ForgeFastExpand : public MainArmyPlay
{
public:
    explicit ForgeFastExpand();

    [[nodiscard]] bool isDefensive() const override { return true; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

    void disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                 const std::function<void(const MyUnit)> &movableUnitCallback) override;

private:
    enum class State
    {
        STATE_PYLON_PENDING,    // Initial state where pylon is not yet built
        STATE_FORGE_PENDING,    // Pylon is built, forge pending
        STATE_NEXUS_PENDING,    // Forge is built, builds reactive cannons before building nexus
        STATE_GATEWAY_PENDING,  // Nexus is built, builds reactive cannons before building gateway
        STATE_FINISHED,         // All buildings are built, defends the wall until strategy engine is ready to transition
        STATE_ANTIFASTRUSH,     // Enemy is doing a fast rush that causes us to abort the FFE and defend the main instead
    };
    State currentState;
    std::shared_ptr<DefendWallSquad> squad;
    std::unique_ptr<WorkerDefenseSquad> mainBaseWorkerDefenseSquad;
};
