#pragma once

#include "MyUnit.h"

class MyCarrier : public MyUnitImpl
{
public:
    explicit MyCarrier(BWAPI::Unit unit);

    void update(BWAPI::Unit unit) override;
};
