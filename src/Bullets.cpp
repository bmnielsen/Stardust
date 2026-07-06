#include "Bullets.h"

#include "Units.h"
#include "Players.h"
#include "NoGoAreas.h"

namespace Bullets
{
    namespace
    {
        // Map of bullet types to unit types for bullets that deal damage after a delay, where the bullet travels to its target
        // We use this for determining damage from bullets that come out of the fog
        // The integer is the initial bullet remove delay, which we use to differentiate between which unit type fired the bullet in cases where
        // the same bullet can be fired from multiple units. This doesn't work for Dragoon vs. Arbiter though since they have the same delay.
        const std::map<BWAPI::BulletType, std::set<std::pair<BWAPI::UnitType, int>>> delayedDamageBulletUnits = {
                {BWAPI::BulletTypes::Burst_Lasers, {{BWAPI::UnitTypes::Terran_Wraith, 254}}},
                {BWAPI::BulletTypes::Gemini_Missiles, {{BWAPI::UnitTypes::Terran_Wraith, 254}}},
                {BWAPI::BulletTypes::Fragmentation_Grenade, {{BWAPI::UnitTypes::Terran_Vulture, 254}}},
                {BWAPI::BulletTypes::Longbolt_Missile, {{BWAPI::UnitTypes::Terran_Missile_Turret, 254}, {BWAPI::UnitTypes::Terran_Goliath, 246}}},
                {BWAPI::BulletTypes::ATS_ATA_Laser_Battery, {{BWAPI::UnitTypes::Terran_Battlecruiser, 254}}},
                {BWAPI::BulletTypes::Yamato_Gun, {{BWAPI::UnitTypes::Terran_Battlecruiser, 254}}},
                {BWAPI::BulletTypes::Halo_Rockets, {{BWAPI::UnitTypes::Terran_Valkyrie, 254}}},
                {BWAPI::BulletTypes::Anti_Matter_Missile, {{BWAPI::UnitTypes::Protoss_Scout, 29}}},
                {BWAPI::BulletTypes::Phase_Disruptor, {{BWAPI::UnitTypes::Protoss_Dragoon, 254}, {BWAPI::UnitTypes::Protoss_Arbiter, 254}}},
                {BWAPI::BulletTypes::STA_STS_Cannon_Overlay, {{BWAPI::UnitTypes::Protoss_Photon_Cannon, 254}}},
                {BWAPI::BulletTypes::Pulse_Cannon, {{BWAPI::UnitTypes::Protoss_Interceptor, 254}}},
                {BWAPI::BulletTypes::Glave_Wurm, {{BWAPI::UnitTypes::Zerg_Mutalisk, 60}}},
                {BWAPI::BulletTypes::Seeker_Spores, {{BWAPI::UnitTypes::Zerg_Spore_Colony, 59}}},
                {BWAPI::BulletTypes::Subterranean_Spines, {{BWAPI::UnitTypes::Zerg_Lurker, 254}}},
                {BWAPI::BulletTypes::Acid_Spore, {{BWAPI::UnitTypes::Zerg_Guardian, 254}}},
                {BWAPI::BulletTypes::Corrosive_Acid_Shot, {{BWAPI::UnitTypes::Zerg_Devourer, 254}}},
        };

        // Map of bullet types to delay frames for bullets that don't have travel time but deal damage after a fixed frame delay
        const std::map<BWAPI::BulletType, std::pair<BWAPI::UnitType, int>> fixedDelayedDamageBullets = {
            {BWAPI::BulletTypes::Arclite_Shock_Cannon_Hit, {BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode, 1}},
            {BWAPI::BulletTypes::Invisible, {BWAPI::UnitTypes::Terran_Firebat, 5}},
            {BWAPI::BulletTypes::Psionic_Shockwave_Hit, {BWAPI::UnitTypes::Protoss_Archon, 3}},
            {BWAPI::BulletTypes::Sunken_Colony_Tentacle, {BWAPI::UnitTypes::Zerg_Sunken_Colony, 5}},
        };

        // Map of bullet types to the tech they infer
        // Includes information about what race uses that tech and whether the tech is usually used on own units or not
        const std::map<BWAPI::BulletType, std::tuple<BWAPI::TechType, BWAPI::Race, bool>> bulletTechTypes = {
                {BWAPI::BulletTypes::Psionic_Storm, {BWAPI::TechTypes::Psionic_Storm, BWAPI::Races::Protoss, false}},
                {BWAPI::BulletTypes::EMP_Missile, {BWAPI::TechTypes::EMP_Shockwave, BWAPI::Races::Terran, false}},
                {BWAPI::BulletTypes::Yamato_Gun, {BWAPI::TechTypes::Yamato_Gun, BWAPI::Races::Terran, false}},
                {BWAPI::BulletTypes::Optical_Flare_Grenade, {BWAPI::TechTypes::Optical_Flare, BWAPI::Races::Terran, false}},
                {BWAPI::BulletTypes::Plague_Cloud, {BWAPI::TechTypes::Plague, BWAPI::Races::Zerg, false}},
                {BWAPI::BulletTypes::Consume, {BWAPI::TechTypes::Consume, BWAPI::Races::Zerg, true}},
                {BWAPI::BulletTypes::Ensnare, {BWAPI::TechTypes::Ensnare, BWAPI::Races::Zerg, false}},
        };


        std::map<int, int> seenBulletFrames;
        int bulletsSeenAtExtendedMarineRange;

        void trackResearch(BWAPI::Bullet bullet)
        {
            // Reference the data for this bullet type, or return if it isn't a bullet that infers tech research
            auto it = bulletTechTypes.find(bullet->getType());
            if (it == bulletTechTypes.end()) return;
            const auto &[techType, race, usedOnOwnUnits] = it->second;

            // Determine the player
            // This may be null if the source of the bullet has died
            auto player = bullet->getPlayer();

            // If the tech is used by a race other than ours, we can infer that the enemy used it (at least until we mind control SCVs or drones)
            if (!player && race != BWAPI::Broodwar->self()->getRace())
            {
                player = BWAPI::Broodwar->enemy();
            }

            // If the bullet has a target unit, we can infer the player based on which player the tech is expected to be used on
            if (!player && bullet->getTarget() && bullet->getTarget()->getPlayer())
            {
                if (bullet->getTarget()->getPlayer() == BWAPI::Broodwar->self())
                {
                    player = usedOnOwnUnits ? BWAPI::Broodwar->self() : BWAPI::Broodwar->enemy();
                }
                else if (bullet->getTarget()->getPlayer() == BWAPI::Broodwar->enemy())
                {
                    player = usedOnOwnUnits ? BWAPI::Broodwar->enemy() : BWAPI::Broodwar->self();
                }
            }

            if (!player) return;

            Players::setHasResearched(player, techType);
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
        return delayedDamageBulletUnits.contains(type);
    }

    std::optional<int> fixedDamageDelay(BWAPI::BulletType type)
    {
        auto it = fixedDelayedDamageBullets.find(type);
        if (it == fixedDelayedDamageBullets.end()) return std::nullopt;

        return it->second.second;
    }

    int upcomingDamage(BWAPI::Bullet bullet)
    {
        // Require target
        if (!bullet->getTarget() || !bullet->getTarget()->getPlayer()) return 0;

        // Validate target position for tracking bullets
        auto trackingBulletInfo = delayedDamageBulletUnits.find(bullet->getType());
        if (trackingBulletInfo != delayedDamageBulletUnits.end() && bullet->getTargetPosition() != bullet->getTarget()->getPosition()) return 0;

        // Get the player owning the bullet
        BWAPI::Player attackingPlayer = bullet->getPlayer();
        if (bullet->getSource())
        {
            attackingPlayer = bullet->getSource()->getPlayer();
        }
        if (!attackingPlayer)
        {
            if (bullet->getTarget()->getPlayer() == BWAPI::Broodwar->self())
            {
                attackingPlayer = BWAPI::Broodwar->enemy();
            }
            else
            {
                attackingPlayer = BWAPI::Broodwar->self();
            }
        }

        // Set weapon override for Yamato
        auto weaponOverride = BWAPI::WeaponTypes::None;
        if (bullet->getType() == BWAPI::BulletTypes::Yamato_Gun)
        {
            weaponOverride = BWAPI::WeaponTypes::Yamato_Gun;
        }

        // Figure out what unit type fired the bullet
        BWAPI::UnitType sourceUnitType = BWAPI::UnitTypes::None;

        if (bullet->getSource())
        {
            sourceUnitType = bullet->getSource()->getType();
        }

        // Ranged bullet that deals damage after a delay
        if (sourceUnitType == BWAPI::UnitTypes::None && trackingBulletInfo != delayedDamageBulletUnits.end())
        {
            for (const auto &unitTypeAndInitialRemoveTimer : trackingBulletInfo->second)
            {
                if (bullet->getSource() && unitTypeAndInitialRemoveTimer.first != bullet->getSource()->getType()) continue;
                if (bullet->getRemoveTimer() != unitTypeAndInitialRemoveTimer.second) continue;

                sourceUnitType = unitTypeAndInitialRemoveTimer.first;
            }

            // If we hit this block, we didn't see the bullet on its first frame and couldn't get an exact match, so just assume it came from
            // the first possibility
            if (sourceUnitType == BWAPI::UnitTypes::None)
            {
                sourceUnitType = trackingBulletInfo->second.begin()->first;
            }
        }

        // Non-ranged bullet that deals damage after a delay
        if (sourceUnitType == BWAPI::UnitTypes::None)
        {
            auto bulletInfo = fixedDelayedDamageBullets.find(bullet->getType());
            if (bulletInfo != fixedDelayedDamageBullets.end())
            {
                sourceUnitType = bulletInfo->second.first;
            }
        }

        if (!sourceUnitType) return 0;

        return Players::attackDamage(attackingPlayer,
                                     sourceUnitType,
                                     bullet->getTarget()->getPlayer(),
                                     bullet->getTarget()->getType(),
                                     weaponOverride);
    }
}
