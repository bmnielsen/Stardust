#pragma once

#include "Play.h"

class HiddenBase : public Play
{
public:
    Base *base;

    HiddenBase() : Play("HiddenBase"), base(nullptr) {}

    void update() override;

    void disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                 const std::function<void(const MyUnit)> &movableUnitCallback) override;

private:
    MyUnit builder;
};
