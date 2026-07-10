#include "Bullets.h"

#include "EnemyBunker.h"
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

        struct BunkerBullet
        {
            explicit BunkerBullet(const BWAPI::Bullet &bullet)
                : id(bullet->getID())
                , firstSeenFrame(currentFrame - 1)
                , position(bullet->getPosition())
            {}

            int id;
            int firstSeenFrame;
            BWAPI::Position position;
        };

        std::map<int, int> seenBulletFrames;
        int bulletsSeenAtExtendedMarineRange;

        // Using a list, as we want to be able to remove bullets from the middle of the list efficiently
        std::list<BunkerBullet> bunkerBullets;

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
        bunkerBullets.clear();
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

                if (!bullet->getSource())
                {
                    bunkerBullets.emplace_front(bullet);
                }
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

    void updateBunkers()
    {
        // This method updates how many marines we think are in enemy bunkers, based on observed bullets and whether the bunker has any of our units
        // inside its range.
        //
        // For all of the logic here, we assume marines aren't stimmed, since we don't really expect stimmed marines in bunkers in PvT. Even if we
        // do encounter it, though, it will just make one marine look like two while it is stimmed, which is fine (it actually reflects its combat
        // value)
        //
        // Procedure used:
        // - Go through the observed bunker-fired bullets and try to match each with a bunker
        //   - If there are multiple bunkers in range, track them on each and we will deduplicate later
        //   - If the bullet could have been fired from the same marine as a previous bullet matched to the bunker, track them as two separate
        //     "rounds"
        //   - Stop processing bullets once we hit the third "round" on a bunker or track a first "round" that is beyond a frame threshold
        //     - Later bullets can be removed from the list
        // - If we have two full rounds of shots from a bunker, count the max shots per round and update the marine count
        // - If we only have one round of shots from a bunker, update with the number of observed shots unless this is lower than the current count
        //   - Rationale: the marines in the bunker fire at different timings because of their order process timers, so we don't want to see the first
        //     bullet and immediately conclude there is only one marine in the bunker
        // - For any bunkers that have no bullets matched, but where we have previously identified it as containing marines, check if it has had a
        //   friendly unit in its range for a while, indicating that it has probably been emptied in the meantime
        //
        // The cooldown of a non-stimmed marine is randomized in the range of [14,17], but there seems to be some instability in how many frames after
        // shooting the bullet is created, so sometimes we see two bullets from the same marine come with as little as 12 frames between them.

        struct MatchedBunkerBullet
        {
            explicit MatchedBunkerBullet(const BunkerBullet &bullet) : id(bullet.id), firstSeenFrame(bullet.firstSeenFrame) {}

            int id;
            int firstSeenFrame;
            int nextRoundSameMarineBulletId = -1; // ID of the bullet in the next round that is matched to the same marine
        };
        struct BunkerBulletMatches
        {
            std::shared_ptr<EnemyBunker> bunker;
            std::vector<MatchedBunkerBullet> firstRoundBullets;
            std::vector<MatchedBunkerBullet> secondRoundBullets;
            bool hasThirdRound = false;

            // Marks the bunker bullet as being fired from this bunker
            // Returns false if the bullet is expired (either is too old to consider for the first round, or would be placed in a third round)
            bool add(const BunkerBullet &bunkerBullet)
            {
                // This check is a bit difficult to do perfectly, since the bullet gets put about 7 pixels inside the target, and knowing whether the
                // enemy has the marine range upgrade is similarly difficult to perfectly detect
                // Since false negatives (not matching a bullet to any bunker) are much worse than false positives (thinking a shot could come from
                // multiple bunkers), we just use a loose distance check here under the assumption that the enemy has range and adding an extra half
                // tile of buffer
                if (bunker->getDistance(bunkerBullet.position) > 216) return false;

                // This is the first bullet assigned to this bunker
                if (firstRoundBullets.empty())
                {
                    // If the bullet is much older than the maximum time between shots, conclude that this bunker has stopped firing and skip this bullet
                    if (bunkerBullet.firstSeenFrame < (currentFrame - 20)) return false;

                    // Otherwise add it as the first shot
                    firstRoundBullets.emplace_back(bunkerBullet);
                    return true;
                }

                // This is not the first bullet assigned to this bunker, so figure out which "round" it fits into
                auto isInNextRound = [&](std::vector<MatchedBunkerBullet> &previousRound, size_t skip = 0)
                {
                    if (skip >= previousRound.size()) return false;

                    if ((previousRound[skip].firstSeenFrame - bunkerBullet.firstSeenFrame) >= 12)
                    {
                        previousRound[skip].nextRoundSameMarineBulletId = bunkerBullet.id;
                        return true;
                    }
                    return false;
                };

                // First check if the bullet could be in the third round, in which case we are done
                if (isInNextRound(secondRoundBullets))
                {
                    hasThirdRound = true;
                    return false;
                }

                // If this bullet doesn't fit into the second round, potentially shift a bullet previously detected to align everything correctly
                while (!isInNextRound(firstRoundBullets, secondRoundBullets.size()))
                {
                    // Add to the first round if there is nothing in the second round
                    if (secondRoundBullets.empty())
                    {
                        firstRoundBullets.emplace_back(bunkerBullet);
                        return true;
                    }

                    // We previously put a bullet in the second round that now prevents a bullet from being matched, so it should have been in the
                    // first round. Clear its related bullet and move it.
                    for (auto &bullet : firstRoundBullets)
                    {
                        if (bullet.nextRoundSameMarineBulletId == secondRoundBullets.front().id) bullet.nextRoundSameMarineBulletId = -1;
                    }
                    firstRoundBullets.emplace_back(std::move(secondRoundBullets.front()));
                    secondRoundBullets.erase(secondRoundBullets.begin());
                }

                // The bullet goes in the second round
                secondRoundBullets.emplace_back(bunkerBullet);
                return true;
            }

            void setMarineCount(int marineCount) const
            {
                // Cap at 4 in case we've misdetected something (e.g. stimmed marines)
                marineCount = std::min(marineCount, 4);

#if CHERRYVIS_ENABLED
                if (bunker->loadedMarines != marineCount)
                {
                    CherryVis::log(bunker->id) << "Updated loaded marines from " << bunker->loadedMarines << " to " << marineCount;
                }
#endif

                bunker->loadedMarines = marineCount;
            }
        };

        // Initialize the bunker data structures
        std::vector<BunkerBulletMatches> bunkers;
        for (auto &unit : Units::allEnemyOfType(BWAPI::UnitTypes::Terran_Bunker))
        {
            if (!unit->completed) continue;

            auto bunker = std::dynamic_pointer_cast<EnemyBunker>(unit);
            if (bunker)
            {
                bunkers.emplace_back(bunker);
            }
        }

        // Go backwards through the bullets and assign them to bunkers
        // Keep track of bullets that get assigned to multiple bunkers
        std::vector<std::pair<int, std::vector<BunkerBulletMatches*>>> bulletsWithMultipleBunkers;
        for (auto it = bunkerBullets.begin(); it != bunkerBullets.end(); )
        {
            std::vector<BunkerBulletMatches*> bunkersMatched;
            for (auto &bunker : bunkers)
            {
                if (bunker.add(*it))
                {
                    bunkersMatched.emplace_back(&bunker);
                }
            }

            if (!bunkersMatched.empty())
            {
                if (bunkersMatched.size() > 1)
                {
                    bulletsWithMultipleBunkers.emplace_back(it->id, std::move(bunkersMatched));
                }
            }
            else if (it->firstSeenFrame < (currentFrame - 51))
            {
                // Clear bullets after 3 times max marine cooldown
                it = bunkerBullets.erase(it);
                continue;
            }

            ++it;
        }

        // Deduplicate bullets assigned to multiple bunkers
        for (auto &[bulletId, bunkerMatches] : bulletsWithMultipleBunkers)
        {
            struct FindResult
            {
                BunkerBulletMatches *bunkerMatch;
                bool inSecondRound;
                int nextRoundSameMarineBulletId;
                int index;
                size_t bunkerCount;

                void removeFromBunker() const
                {
                    auto &vec =
                        inSecondRound
                        ? bunkerMatch->secondRoundBullets
                        : bunkerMatch->firstRoundBullets;
                    vec.erase(vec.begin() + index);

                    if (nextRoundSameMarineBulletId != -1)
                    {
                        for (auto it = bunkerMatch->secondRoundBullets.begin(); it != bunkerMatch->secondRoundBullets.end(); ++it)
                        {
                            if (it->id == nextRoundSameMarineBulletId)
                            {
                                bunkerMatch->secondRoundBullets.erase(it);
                                break;
                            }
                        }
                    }
                }
            };

            // First pass: find the bullet in each bunker
            std::vector<FindResult> findResults;
            bool anyInSecondRound = false;
            bool anyHaveNextRoundSameMarineBulletId = false;
            for (auto &bunker : bunkerMatches)
            {
                auto find = [&](const std::vector<MatchedBunkerBullet> &bullets)
                {
                    for (int i = 0; i < bullets.size(); ++i)
                    {
                        if (bullets[i].id == bulletId)
                        {
                            return i;
                        }
                    }
                    return -1;
                };

                int firstRound = find(bunker->firstRoundBullets);
                if (firstRound != -1)
                {
                    anyHaveNextRoundSameMarineBulletId =
                        anyHaveNextRoundSameMarineBulletId || (bunker->firstRoundBullets[firstRound].nextRoundSameMarineBulletId != -1);
                    findResults.emplace_back(bunker,
                                             false,
                                             bunker->firstRoundBullets[firstRound].nextRoundSameMarineBulletId,
                                             firstRound,
                                             bunker->firstRoundBullets.size());
                    continue;
                }

                int secondRound = find(bunker->secondRoundBullets);
                if (secondRound != -1)
                {
                    anyInSecondRound = true;
                    findResults.emplace_back(bunker,
                                             true,
                                             -1,
                                             secondRound,
                                             bunker->secondRoundBullets.size());
                    continue;
                }

                // If we fall through to here, it means a matched first round bullet has been removed in an earlier iteration, so we don't have to
                // consider this bunker match
            }

            if (findResults.empty()) continue;

            // Second pass: remove the bullet from any bunkers where:
            // - the bullet is in the second round in some other bunker and in the first round in this one
            // - the bullet has matched to a second round bullet in some bunker and not in this one
            // Keep track of which kept bunker has the least bullets during this scan
            size_t minCount = UINT32_MAX;
            for (auto it = findResults.begin(); it != findResults.end(); )
            {
                if ((anyInSecondRound && !it->inSecondRound) || (anyHaveNextRoundSameMarineBulletId && it->nextRoundSameMarineBulletId == -1))
                {
                    it->removeFromBunker();
                    it = findResults.erase(it);
                }
                else
                {
                    minCount = std::min(minCount, it->bunkerCount);
                    ++it;
                }
            }

            // Third pass: remove the bullet from any bunkers that have a higher count than the minimum
            for (auto it = findResults.begin(); it != findResults.end(); )
            {
                if (it->bunkerCount > minCount)
                {
                    it->removeFromBunker();
                    it = findResults.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            // Final pass: remove the bullet from random bunkers until there is only one left
            for (auto it = findResults.begin() + 1; it != findResults.end(); ++it)
            {
                it->removeFromBunker();
            }
        }

        // Update the marine counts on the bunkers
        for (auto &bunker : bunkers)
        {
            if (bunker.firstRoundBullets.empty())
            {
                // If one of our units has been in range of the bunker for more than 15 frames, assume there are no longer any marines in it
                if (bunker.bunker->myUnitInRange && bunker.bunker->frameMyUnitInRangeLastChanged < (currentFrame - 15))
                {
                    bunker.setMarineCount(0);
                }
            }
            else if (bunker.secondRoundBullets.empty())
            {
                // We are seeing the first shots from the bunker, so bump up the count if needed but otherwise keep the current value
                bunker.setMarineCount(std::max(bunker.bunker->loadedMarines, (int)bunker.firstRoundBullets.size()));
            }
            else
            {
                // We have two rounds of shots, so consider the estimate to be whichever round has the most shots
                // However, as the differences in possible cooldowns can cause some misdetections, we don't reduce an existing count in the following
                // situations:
                // - We haven't seen a third round yet, so we don't have data for two full rounds
                // - We have been in range of this bunker for quite a while, so likely there is just an unlucky volley making us think there are
                //   fewer marines all of a sudden
                // We might still get unlucky and see an ambiguous volley at exactly the wrong time, but we probably won't misdetect for many frames,
                // at least
                if (bunker.hasThirdRound && ((!bunker.bunker->myUnitInRange && bunker.bunker->frameMyUnitInRangeLastChanged > (currentFrame - 15)) ||
                    (bunker.bunker->myUnitInRange && bunker.bunker->frameMyUnitInRangeLastChanged > (currentFrame - 51))))
                {
                    bunker.setMarineCount(std::max(bunker.firstRoundBullets.size(), bunker.secondRoundBullets.size()));
                }
                else
                {
                    bunker.setMarineCount(std::max({
                        bunker.bunker->loadedMarines,
                        (int)bunker.firstRoundBullets.size(),
                        (int)bunker.secondRoundBullets.size()}));
                }
            }
        }
    }
}
