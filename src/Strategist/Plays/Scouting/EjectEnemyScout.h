#pragma once

#include "Play.h"

class EjectEnemyScout : public Play
{
public:
    EjectEnemyScout()
            : Play("EjectEnemyScout")
            , scout(nullptr)
            , dragoon(nullptr)
            , hasHadScout(false) {}

    void update() override;

    void disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                 const std::function<void(const MyUnit)> &movableUnitCallback) override
    {
        if (dragoon) movableUnitCallback(dragoon);
    }

    void addUnit(const MyUnit &unit) override;

    void removeUnit(const MyUnit &unit) override
    {
        dragoon = nullptr;
    }

    // Whether the play has ejected a scout
    bool hasEjectedScout() const;

private:
    Unit scout;
    MyUnit dragoon;

    bool hasHadScout;
};
