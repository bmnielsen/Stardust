#include "Bullets.h"

#include "Units.h"

namespace Bullets
{
    namespace
    {
        std::set<int> seenBullets;
    }

    void initialize()
    {
        seenBullets.clear();
    }

    void update()
    {
        for (auto bullet : BWAPI::Broodwar->getBullets())
        {
            if (seenBullets.find(bullet->getID()) == seenBullets.end())
            {
                Units::onBulletCreate(bullet);

                seenBullets.insert(bullet->getID());
            }
        }
    }
}