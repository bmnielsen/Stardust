#pragma once

#include "Play.h"

class HiddenBase : public Play
{
public:
    Base *base;

    HiddenBase() : Play("HiddenBase"), base(nullptr) {}

    void update() override;

private:
    MyUnit builder;
};
