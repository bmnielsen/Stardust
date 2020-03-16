#pragma once

#include "Play.h"
#include "Squads/DefendBaseSquad.h"

class DefendBase : public Play
{
public:
    explicit DefendBase(Base *base);

    [[nodiscard]] const char *label() const override { return "DefendBase"; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

protected:
    Base *base;
    std::shared_ptr<DefendBaseSquad> squad;

    std::vector<MyUnit> reservedWorkers;

    void mineralLineWorkerDefense(std::set<Unit> &enemiesInBase);
};
