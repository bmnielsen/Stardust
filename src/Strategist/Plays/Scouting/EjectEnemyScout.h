#pragma once

#include "Play.h"

class EjectEnemyScout : public PlayItem
{
public:
    EjectEnemyScout()
            : PlayItem("EjectEnemyScout")
            , scout(nullptr)
            , dragoon(nullptr)
            , hasHadScout(false) {}

    void update() override;

    void disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                 const std::function<void(const OwnedUnit)> &movableUnitCallback) override
    {
        if (dragoon) movableUnitCallback(dragoon);
    }

    void addUnit(const OwnedUnit &gameUnit) override;

    void removeUnit(const OwnedUnit &gameUnit) override
    {
        dragoon = nullptr;
    }

    /* Whether the play has ejected a scout */
    bool hasEjectedScout() const;

private:
    Unit scout;
    OwnedUnit dragoon;

    bool hasHadScout;
};
