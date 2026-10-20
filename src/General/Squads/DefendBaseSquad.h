#pragma once

#include "Squad.h"
#include "Base.h"
#include "Unit.h"
#include "WorkerDefenseSquad.h"

class DefendBaseSquad : public UnitSquad
{
public:
    explicit DefendBaseSquad(Base *baseLoc)
            : UnitSquad((std::ostringstream() << "Defend base @ " << baseLoc->getTilePosition()).str())
            , baseLoc(baseLoc)
            , workerDefenseSquad(std::make_unique<WorkerDefenseSquad>(baseLoc))
    {
        targetPosition = baseLoc->getPosition();
    };

    virtual ~DefendBaseSquad() = default;

    Base* baseLoc;
    std::set<Unit> enemyUnits;

    void execute() override;

    void disband() override { workerDefenseSquad->disband(); }

private:
    std::unique_ptr<WorkerDefenseSquad> workerDefenseSquad;

    void execute(UnitCluster &clump) override;
};
