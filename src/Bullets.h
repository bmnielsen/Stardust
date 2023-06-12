#pragma once

#include "Common.h"

namespace Bullets
{
    void initialize();

    void update();

    bool dealsDamageAfterDelay(BWAPI::BulletType type);

    int upcomingDamage(BWAPI::Bullet bullet);
}
