#pragma once

#include "Common.h"
#include <optional>

namespace Bullets
{
    void initialize();

    void update();

    bool dealsDamageAfterDelay(const BWAPI::BulletType type);

    std::optional<int> fixedDamageDelay(BWAPI::BulletType type);

    int upcomingDamage(BWAPI::Bullet bullet);

    // Updates how many marines we think are in each of the enemy's bunkers based on observed bullets
    void updateBunkers();
}
