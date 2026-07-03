#pragma once

#include "Common.h"

namespace Bullets
{
    void initialize();

    void update();

    bool dealsDamageAfterDelay(const BWAPI::BulletType type);

    std::optional<int> fixedDamageDelay(BWAPI::BulletType type);

    int upcomingDamage(BWAPI::Bullet bullet);
}
