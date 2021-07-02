#pragma once

#include "Play.h"

class HiddenBase : public Play
{
public:
    HiddenBase() : Play("HiddenBase"), base(nullptr) {}

    void update() override;

private:
    Base *base;
    MyUnit builder;
};
