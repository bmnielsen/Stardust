#include "Bullets.h"

#include "Units.h"
#include "Players.h"

namespace Bullets
{
    namespace
    {
        std::map<int, int> seenBulletFrames;
        int bulletsSeenAtExtendedMarineRange;

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

            // Call onBulletCreate on the first frame the bullet is seen
            auto frame = seenBulletFrames.find(bullet->getID());
            if (frame == seenBulletFrames.end())
            {
                Units::onBulletCreate(bullet);
                seenBulletFrames[bullet->getID()] = BWAPI::Broodwar->getFrameCount();
                return;
            }

            // For marine rifle hits, check if we can deduce whether or not the enemy has the range upgrade for shots from a bunker
            // We check on the frame after we first see the bullet, since this is the frame hidden units appear from the fog
            if (bullet->getType() == BWAPI::BulletTypes::Gauss_Rifle_Hit &&
                frame->second == (BWAPI::Broodwar->getFrameCount() - 1))
            {
                checkBunkerRange(bullet);
            }
        }
    }
}