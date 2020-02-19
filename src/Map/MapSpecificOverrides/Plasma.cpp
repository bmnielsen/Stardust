#include "Plasma.h"

void Plasma::initializeChokes(std::map<const BWEM::ChokePoint *, Choke *> &chokes)
{
    for (auto &pair : chokes)
    {
        const BWEM::ChokePoint *choke = pair.first;
        Choke &chokeData = *pair.second;

        BWAPI::Position chokeCenter(choke->Center());

        // Determine if the choke is blocked by eggs, and grab the close mineral patches
        bool blockedByEggs = false;
        BWAPI::Unit closestMineralPatch = nullptr;
        BWAPI::Unit secondClosestMineralPatch = nullptr;
        int closestMineralPatchDist = INT_MAX;
        int secondClosestMineralPatchDist = INT_MAX;
        for (const auto staticNeutral : BWAPI::Broodwar->getStaticNeutralUnits())
        {
            if (!blockedByEggs && staticNeutral->getType() == BWAPI::UnitTypes::Zerg_Egg &&
                staticNeutral->getDistance(chokeCenter) < 100)
            {
                blockedByEggs = true;
            }

            if (staticNeutral->getType() == BWAPI::UnitTypes::Resource_Mineral_Field &&
                staticNeutral->getResources() == 32)
            {
                int dist = staticNeutral->getDistance(chokeCenter);
                if (dist <= closestMineralPatchDist)
                {
                    secondClosestMineralPatchDist = closestMineralPatchDist;
                    closestMineralPatchDist = dist;
                    secondClosestMineralPatch = closestMineralPatch;
                    closestMineralPatch = staticNeutral;
                }
                else if (dist < secondClosestMineralPatchDist)
                {
                    secondClosestMineralPatchDist = dist;
                    secondClosestMineralPatch = staticNeutral;
                }
            }
        }

        if (!blockedByEggs) continue;

        chokeData.requiresMineralWalk = true;

        auto closestArea = BWEM::Map::Instance().GetNearestArea(
                BWAPI::WalkPosition(closestMineralPatch->getTilePosition()) + BWAPI::WalkPosition(4, 2));
        auto secondClosestArea = BWEM::Map::Instance().GetNearestArea(
                BWAPI::WalkPosition(secondClosestMineralPatch->getTilePosition()) + BWAPI::WalkPosition(4, 2));
        if (closestArea == choke->GetAreas().second &&
            secondClosestArea == choke->GetAreas().first)
        {
            chokeData.secondAreaMineralPatch = closestMineralPatch;
            chokeData.firstAreaMineralPatch = secondClosestMineralPatch;
        }
        else
        {
            // Note: Two of the chokes don't have the mineral patches show up in expected areas because of
            // suboptimal BWEM choke placement, but luckily they both follow this pattern
            chokeData.firstAreaMineralPatch = closestMineralPatch;
            chokeData.secondAreaMineralPatch = secondClosestMineralPatch;
        }
    }
}