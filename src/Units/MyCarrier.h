#pragma once

#include "MyUnit.h"

class MyCarrier : public MyUnitImpl
{
public:
    explicit MyCarrier(BWAPI::Unit gameUnit);

    void update(BWAPI::Unit gameUnit) override;
};
