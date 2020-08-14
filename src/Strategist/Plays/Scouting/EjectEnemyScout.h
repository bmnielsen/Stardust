#pragma once

#include "Play.h"

class EjectEnemyScout : public Play
{
public:
    EjectEnemyScout()
            : Play("EjectEnemyScout")
            , scout(nullptr)
            , dragoon(nullptr) {}

    void update() override;

    void disband(const std::function<void(const MyUnit &)> &removedUnitCallback,
                 const std::function<void(const MyUnit &)> &movableUnitCallback) override
    {
        if (dragoon) movableUnitCallback(dragoon);
    }

    void addUnit(MyUnit unit) override
    {
        dragoon = unit;
    }

    void removeUnit(MyUnit unit) override
    {
        dragoon = nullptr;
    }

private:
    Unit scout;
    MyUnit dragoon;

    bool findScout();
};
