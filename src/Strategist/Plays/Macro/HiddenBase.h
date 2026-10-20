#pragma once

#include "Play.h"

class HiddenBase : public PlayItem
{
public:
    Base *baseLoc;

    HiddenBase() : PlayItem("HiddenBase"), baseLoc(nullptr) {}

    void update() override;

    void disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                 const std::function<void(const OwnedUnit)> &movableUnitCallback) override;

private:
    OwnedWorker builder;
};
