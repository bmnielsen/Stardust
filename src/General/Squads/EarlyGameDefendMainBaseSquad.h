#pragma once

#include "Squad.h"
#include "Choke.h"
#include "WorkerDefenseSquad.h"

class EarlyGameDefendMainBaseSquad : public Squad
{
public:
    EarlyGameDefendMainBaseSquad();

    virtual ~EarlyGameDefendMainBaseSquad() = default;

    [[nodiscard]] bool canTransitionToAttack() const;

    void execute() override;

    void disband() override { workerDefenseSquad->disband(); }

private:
    Choke *choke;
    BWAPI::Position chokeDefendEnd;
    std::unique_ptr<WorkerDefenseSquad> workerDefenseSquad;

    void initializeChoke();

    void execute(UnitCluster &cluster) override;

    [[nodiscard]] bool canAddUnitToCluster(const MyUnit &unit, const std::shared_ptr<UnitCluster> &cluster, int dist) const override;

    [[nodiscard]] bool shouldCombineClusters(const std::shared_ptr<UnitCluster> &first, const std::shared_ptr<UnitCluster> &second) const override;

    [[nodiscard]] bool shouldRemoveFromCluster(const MyUnit &unit, const std::shared_ptr<UnitCluster> &cluster) const override;
};
