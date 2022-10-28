#include "MyCannon.h"

bool MyCannon::isReady() const
{
    if (!MyUnitImpl::isReady()) return false;
    if (issuedOrderThisFrame) return false;

    // We aren't ready while on cooldown, adjusted for latency
    if (cooldownUntil > (currentFrame + BWAPI::Broodwar->getRemainingLatencyFrames() + 1)) return false;

    // We aren't ready if we have just sent the attack command
    if (bwapiUnit->getLastCommand().type == BWAPI::UnitCommandTypes::Attack_Unit &&
        lastCommandFrame >= (currentFrame - BWAPI::Broodwar->getLatencyFrames()))
    {
        return false;
    }

    return true;
}

void MyCannon::attackUnit(const Unit &target,
                           std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                           bool clusterAttacking,
                           int enemyAoeRadius)
{
    // Abort the attack if the target isn't visible
    if (!target->exists() || !target->bwapiUnit->isVisible()) return;

    // We always force the attack to reset the order process timer (isReady ensures we don't resend too often)
    attack(target->bwapiUnit, true);
}
