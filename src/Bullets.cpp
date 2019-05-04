#include "Bullets.h"

#include "Units.h"

namespace Bullets
{
#ifndef _DEBUG
    namespace
    {
#endif

        std::set<int> seenBullets;

#ifndef _DEBUG
    }
#endif

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