#pragma once

#include "Squad.h"
#include "Base.h"
#include "Unit.h"
#include "WorkerDefenseSquad.h"

class DefendBaseSquad : public Squad
{
public:
    explicit DefendBaseSquad(Base *base)
            : Squad((std::ostringstream() << "Defend base @ " << base->getTilePosition()).str())
            , workerDefenseSquad(std::make_unique<WorkerDefenseSquad>(base))
    {
        targetPosition = base->getPosition();
    };

    virtual ~DefendBaseSquad() = default;

    std::set<Unit> enemyUnits;

    void execute() override;

    void disband() override { workerDefenseSquad->disband(); }

private:
    std::unique_ptr<WorkerDefenseSquad> workerDefenseSquad;

    void execute(UnitCluster &cluster) override;
};
