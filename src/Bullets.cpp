#include "Bullets.h"

#include "Units.h"
#include "Players.h"
#include "NoGoAreas.h"

namespace Bullets
{
    namespace
    {
        // Map of bullet types to unit types for bullets that deal damage after a delay
        // We use this for determining damage from bullets that come out of the fog
        const std::map<BWAPI::BulletType, std::set<std::pair<BWAPI::UnitType, int>>> delayedDamageBulletUnits = {
                {BWAPI::BulletTypes::Burst_Lasers, {{BWAPI::UnitTypes::Terran_Wraith, 254}}},
                {BWAPI::BulletTypes::Gemini_Missiles, {{BWAPI::UnitTypes::Terran_Wraith, 254}}},
                {BWAPI::BulletTypes::Fragmentation_Grenade, {{BWAPI::UnitTypes::Terran_Vulture, 254}}},
                {BWAPI::BulletTypes::Longbolt_Missile, {{BWAPI::UnitTypes::Terran_Missile_Turret, 254}, {BWAPI::UnitTypes::Terran_Goliath, 246}}},
                {BWAPI::BulletTypes::ATS_ATA_Laser_Battery, {{BWAPI::UnitTypes::Terran_Battlecruiser, 254}}},
                {BWAPI::BulletTypes::Yamato_Gun, {{BWAPI::UnitTypes::Terran_Battlecruiser, 254}}},
                {BWAPI::BulletTypes::Halo_Rockets, {{BWAPI::UnitTypes::Terran_Valkyrie, 254}}},
                {BWAPI::BulletTypes::Anti_Matter_Missile, {{BWAPI::UnitTypes::Protoss_Scout, 29}}},
                {BWAPI::BulletTypes::Phase_Disruptor, {{BWAPI::UnitTypes::Protoss_Dragoon, 254}}},
                {BWAPI::BulletTypes::STA_STS_Cannon_Overlay, {{BWAPI::UnitTypes::Protoss_Photon_Cannon, 254}}},
                {BWAPI::BulletTypes::Pulse_Cannon, {{BWAPI::UnitTypes::Protoss_Interceptor, 254}}},
                {BWAPI::BulletTypes::Glave_Wurm, {{BWAPI::UnitTypes::Zerg_Mutalisk, 60}}},
                {BWAPI::BulletTypes::Seeker_Spores, {{BWAPI::UnitTypes::Zerg_Spore_Colony, 59}}},
                {BWAPI::BulletTypes::Subterranean_Spines, {{BWAPI::UnitTypes::Zerg_Lurker, 254}}},
        };

        std::map<int, int> seenBulletFrames;
        int bulletsSeenAtExtendedMarineRange;

        void trackResearch(BWAPI::Bullet bullet)
        {
            // Terran
            if (bullet->getType() == BWAPI::BulletTypes::EMP_Missile)
            {
                Players::setHasResearched(bullet->getPlayer(), BWAPI::TechTypes::EMP_Shockwave);
            }
            if (bullet->getType() == BWAPI::BulletTypes::Yamato_Gun)
            {
                Players::setHasResearched(bullet->getPlayer(), BWAPI::TechTypes::Yamato_Gun);
            }
            if (bullet->getType() == BWAPI::BulletTypes::Optical_Flare_Grenade)
            {
                Players::setHasResearched(bullet->getPlayer(), BWAPI::TechTypes::Optical_Flare);
            }

            // Zerg
            if (bullet->getType() == BWAPI::BulletTypes::Plague_Cloud)
            {
                Players::setHasResearched(bullet->getPlayer(), BWAPI::TechTypes::Plague);
            }
            if (bullet->getType() == BWAPI::BulletTypes::Consume)
            {
                Players::setHasResearched(bullet->getPlayer(), BWAPI::TechTypes::Consume);
            }
            if (bullet->getType() == BWAPI::BulletTypes::Ensnare)
            {
                Players::setHasResearched(bullet->getPlayer(), BWAPI::TechTypes::Ensnare);
            }

            // Protoss
            if (bullet->getType() == BWAPI::BulletTypes::Psionic_Storm)
            {
                Players::setHasResearched(bullet->getPlayer(), BWAPI::TechTypes::Psionic_Storm);
            }
        }

        void checkBunkerRange(BWAPI::Bullet bullet)
        {
            // Bail out if we already know the enemy has the upgrade
            if (Players::weaponRange(BWAPI::Broodwar->enemy(), BWAPI::WeaponTypes::Gauss_Rifle) == 160) return;

            // If the bullet has a source, it definitely isn't from a bunker
            // The bullet may still be from a marine if it has died in the meantime, but since we
            // analyze on the second frame, this is unlikely to happen
            if (bullet->getSource()) return;

            // Ignore bullets where the target has died in the meantime
            if (!bullet->getTarget()) return;

            // Get the closest visible bunker to the bullet
            int bestDist = INT_MAX;
            BWAPI::Unit bunker = nullptr;
            for (auto &unit : BWAPI::Broodwar->enemy()->getUnits())
            {
                if (!unit->exists() || !unit->isVisible() || !unit->isCompleted()) continue;
                if (unit->getType() != BWAPI::UnitTypes::Terran_Bunker) continue;
                int dist = unit->getDistance(bullet->getPosition());
                if (dist < bestDist)
                {
                    bestDist = dist;
                    bunker = unit;
                }
            }
            if (!bunker) return;

            // The bullet seems to always be located 7 pixels "inside" the target, so use this to compute distance between bunker and target
            bestDist -= 7;

            // Now use this to determine if the marines have the range upgrade
            // We get some false positives, so use a relatively conservative distance range and
            // make sure we have seen a few volleys
            if (bestDist >= 190 && bestDist <= 192)
            {
                bulletsSeenAtExtendedMarineRange++;
                if (bulletsSeenAtExtendedMarineRange > 4)
                {
                    CherryVis::log() << "Detected ranged marines in bunker @ " << BWAPI::WalkPosition(bunker->getTilePosition())
                                     << "; target @ " << BWAPI::WalkPosition(bullet->getTarget()->getTilePosition()) << "; dist=" << bestDist;
                    Log::Get() << "Detected ranged marines in bunker @ " << bunker->getTilePosition()
                               << "; target @ " << bullet->getTarget()->getTilePosition() << "; dist=" << bestDist;
                    Players::setWeaponRange(BWAPI::Broodwar->enemy(), BWAPI::WeaponTypes::Gauss_Rifle, 160);
                }
            }
        }
    }

    void initialize()
    {
        seenBulletFrames.clear();
        bulletsSeenAtExtendedMarineRange = 0;
    }

    void update()
    {
        for (auto bullet : BWAPI::Broodwar->getBullets())
        {
            // Ignore invalid bullets
            if (!bullet->exists() || !bullet->isVisible() ||
                (!bullet->getSource() && !bullet->getTarget()))
            {
                continue;
            }

            // Track enemy research
            trackResearch(bullet);

            // Call onBulletCreate on the first frame the bullet is seen
            auto frame = seenBulletFrames.find(bullet->getID());
            if (frame == seenBulletFrames.end())
            {
                Units::onBulletCreate(bullet);
                NoGoAreas::onBulletCreate(bullet);
                seenBulletFrames[bullet->getID()] = currentFrame;
                continue;
            }

            // For marine rifle hits, check if we can deduce whether or not the enemy has the range upgrade for shots from a bunker
            // We check on the frame after we first see the bullet, since this is the frame hidden units appear from the fog
            if (bullet->getType() == BWAPI::BulletTypes::Gauss_Rifle_Hit &&
                frame->second == (currentFrame - 1))
            {
                checkBunkerRange(bullet);
            }
        }
    }

    bool dealsDamageAfterDelay(BWAPI::BulletType type)
    {
        return delayedDamageBulletUnits.find(type) != delayedDamageBulletUnits.end();
    }

    int upcomingDamage(BWAPI::Bullet bullet)
    {
        // Require target
        if (!bullet->getTarget() || !bullet->getTarget()->getPlayer()) return 0;

        // Get the player owning the bullet
        BWAPI::Player attackingPlayer = bullet->getPlayer();
        if (bullet->getSource())
        {
            attackingPlayer = bullet->getSource()->getPlayer();
        }
        if (!attackingPlayer) return 0;

        // Set weapon override for Yamato
        auto weaponOverride = BWAPI::WeaponTypes::None;
        if (bullet->getType() == BWAPI::BulletTypes::Yamato_Gun)
        {
            weaponOverride = BWAPI::WeaponTypes::Yamato_Gun;
        }

        // Require ranged bullet that deals damage after a delay
        auto bulletInfo = delayedDamageBulletUnits.find(bullet->getType());
        if (bulletInfo == delayedDamageBulletUnits.end()) return 0;

        BWAPI::UnitType sourceUnitType = BWAPI::UnitTypes::None;
        for (const auto &unitTypeAndInitialRemoveTimer : bulletInfo->second)
        {
            if (bullet->getSource() && unitTypeAndInitialRemoveTimer.first != bullet->getSource()->getType()) continue;
            if (bullet->getRemoveTimer() != unitTypeAndInitialRemoveTimer.second) continue;

            sourceUnitType = unitTypeAndInitialRemoveTimer.first;
        }

        // If we hit this block, we didn't see the bullet on its first frame and couldn't get an exact match
        if (sourceUnitType == BWAPI::UnitTypes::None)
        {
            if (bullet->getSource())
            {
                return Players::attackDamage(attackingPlayer,
                                             bullet->getSource()->getType(),
                                             bullet->getTarget()->getPlayer(),
                                             bullet->getTarget()->getType(),
                                             weaponOverride);
            }

            return Players::attackDamage(attackingPlayer,
                                         bulletInfo->second.begin()->first,
                                         bullet->getTarget()->getPlayer(),
                                         bullet->getTarget()->getType(),
                                         weaponOverride);
        }

        if (bullet->getTargetPosition() != bullet->getTarget()->getPosition()) return 0;

        return Players::attackDamage(attackingPlayer,
                                     sourceUnitType,
                                     bullet->getTarget()->getPlayer(),
                                     bullet->getTarget()->getType(),
                                     weaponOverride);
    }
}
