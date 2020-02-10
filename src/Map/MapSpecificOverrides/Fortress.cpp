#include "Fortress.h"

void Fortress::initializeChokes(std::map<const BWEM::ChokePoint *, Choke *> &chokes)
{
    for (auto &pair : chokes)
    {
        const BWEM::ChokePoint *choke = pair.first;
        Choke &chokeData = *pair.second;

        // On Fortress the mineral walking chokes are all considered blocked by BWEM
        if (!choke->Blocked()) continue;

        chokeData.requiresMineralWalk = true;

        // Find the two closest mineral patches to the choke
        BWAPI::Position chokeCenter(choke->Center());
        BWAPI::Unit closestMineralPatch = nullptr;
        BWAPI::Unit secondClosestMineralPatch = nullptr;
        int closestMineralPatchDist = INT_MAX;
        int secondClosestMineralPatchDist = INT_MAX;
        for (const auto staticNeutral : BWAPI::Broodwar->getStaticNeutralUnits())
        {
            if (staticNeutral->getType().isMineralField())
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

        // Each entrance to a mineral walking base has two doors with a mineral patch behind each
        // So the choke closest to the base will have a mineral patch on both sides we can use
        // The other choke has a mineral patch on the way in, but not on the way out, so one will be null
        // We will use a random visible mineral patch on the map to handle getting out
        auto closestArea = BWEM::Map::Instance().GetNearestArea(BWAPI::WalkPosition(closestMineralPatch->getTilePosition()) + BWAPI::WalkPosition(4, 2));
        auto secondClosestArea = BWEM::Map::Instance().GetNearestArea(
                BWAPI::WalkPosition(secondClosestMineralPatch->getTilePosition()) + BWAPI::WalkPosition(4, 2));

        if (closestArea == choke->GetAreas().first)
            chokeData.firstAreaMineralPatch = closestMineralPatch;
        if (closestArea == choke->GetAreas().second)
            chokeData.secondAreaMineralPatch = closestMineralPatch;
        if (secondClosestArea == choke->GetAreas().first)
            chokeData.firstAreaMineralPatch = secondClosestMineralPatch;
        if (secondClosestArea == choke->GetAreas().second)
            chokeData.secondAreaMineralPatch = secondClosestMineralPatch;

        // We use the door as the starting point regardless of which side is which
        chokeData.firstAreaStartPosition = choke->BlockingNeutral()->Unit()->getInitialPosition();
        chokeData.secondAreaStartPosition = choke->BlockingNeutral()->Unit()->getInitialPosition();
    }
}
