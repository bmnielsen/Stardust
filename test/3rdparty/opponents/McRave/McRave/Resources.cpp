#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Resources {

    namespace {

        set<shared_ptr<ResourceInfo>> myMinerals;
        set<shared_ptr<ResourceInfo>> myGas;
        set<shared_ptr<ResourceInfo>> myBoulders;
        bool mineralSat, gasSat, halfMineralSat, halfGasSat;
        int miners, gassers;
        int mineralCount, gasCount;
        int incomeMineral, incomeGas;
        int maxGas;
        int maxMin;
        string mapName, myRaceChar;

        void updateIncome(const shared_ptr<ResourceInfo>& r)
        {
            auto &resource = *r;
            auto cnt = resource.getGathererCount();
            if (resource.getType().isMineralField())
                incomeMineral += cnt == 1 ? 65 : 126;
            else
                incomeGas += resource.getRemainingResources() ? 103 * cnt : 26 * cnt;
        }

        void updateInformation(const shared_ptr<ResourceInfo>& r)
        {
            auto &resource = *r;
            if (resource.unit()->exists())
                resource.updateResource();

            UnitType geyserType = Broodwar->self()->getRace().getRefinery();

            // If resource is blocked from usage
            if (!resource.getType().isMineralField() && resource.getTilePosition().isValid()) {
                for (auto block = mapBWEM.GetTile(resource.getTilePosition()).GetNeutral(); block; block = block->NextStacked()) {
                    if (block && block->Unit() && block->Unit()->exists() && block->Unit()->isInvincible() && !block->IsGeyser())
                        resource.setResourceState(ResourceState::None);
                }
            }

            // Update resource state
            if (resource.hasStation()) {
                auto base = Util::getClosestUnit(resource.getPosition(), PlayerState::Self, [&](auto &u) {
                    return u->getType().isResourceDepot() && u->getPosition() == resource.getStation()->getBase()->Center();
                });

                resource.setResourceState(ResourceState::None);
                if (base) {
                    if (base->unit()->getRemainingBuildTime() < 150 || base->getType() == Zerg_Lair || base->getType() == Zerg_Hive || (resource.getType().isRefinery() && resource.unit()->getRemainingBuildTime() < 120))
                        resource.setResourceState(ResourceState::Mineable);
                    else
                        resource.setResourceState(ResourceState::Assignable);
                }
            }

            // Update saturation            
            if (resource.getType().isMineralField() && resource.getResourceState() != ResourceState::None)
                miners += resource.getGathererCount();
            else if (resource.getType() == geyserType && resource.unit()->isCompleted() && resource.getResourceState() != ResourceState::None)
                gassers += resource.getGathererCount();

            auto trackSaturationAt = Players::ZvZ() ? 2 : 3;
            if (!resource.isBoulder()) {
                if (resource.getResourceState() == ResourceState::Mineable || (resource.getResourceState() == ResourceState::Assignable && int(Stations::getStations(PlayerState::Self).size()) >= trackSaturationAt)) {
                    resource.getType().isMineralField() ? mineralCount++ : gasCount++;
                    resource.getType().isMineralField() ? maxMin+=resource.getWorkerCap() : maxGas+=resource.getWorkerCap();
                }

                for (auto &w : resource.targetedByWhat()) {
                    if (auto worker = w.lock()) {
                        if (worker->getBuildPosition().isValid())
                             maxMin--, maxGas--;
                    }
                }
            }
        }

        void updateResources()
        {
            mineralCount = 0;
            gasCount = 0;
            miners = 0;
            gassers = 0;
            maxGas = 0;
            maxMin = 0;

            const auto update = [&](const shared_ptr<ResourceInfo>& r) {
                updateInformation(r);
                updateIncome(r);
            };

            for (auto &r : myBoulders)
                update(r);
            for (auto &r : myMinerals)
                update(r);
            for (auto &r : myGas)
                update(r);

            mineralSat = miners >= maxMin;
            halfMineralSat = miners >= mineralCount;
            gasSat = gassers >= maxGas;
            halfGasSat = gassers >= gasCount;
        }
    }

    void recheckSaturation()
    {
        miners = 0;
        gassers = 0;

        for (auto &r : myMinerals) {
            auto &resource = *r;
            miners += resource.getGathererCount();
        }

        for (auto &r : myGas) {
            auto &resource = *r;
            gassers += resource.getGathererCount();
        }

        mineralSat = miners >= maxMin;
        halfMineralSat = miners >= mineralCount;
        gasSat = gassers >= maxGas;
        halfGasSat = gassers >= gasCount;
    }

    void onFrame()
    {
        Visuals::startPerfTest();
        updateResources();
        Visuals::endPerfTest("Resources");
    }

    void onStart()
    {
        // Store all resources
        for (auto &resource : Broodwar->getMinerals())
            storeResource(resource);
        for (auto &resource : Broodwar->getGeysers())
            storeResource(resource);

        // Grab only the alpha characters from the map name to remove version numbers
        for (auto &c : Broodwar->mapFileName()) {
            if (isalpha(c))
                mapName.push_back(c);
            if (c == '.')
                break;
        }

        myRaceChar = *Broodwar->self()->getRace().c_str();
        ifstream readFileA("bwapi-data/AI/" + mapName + "GatherInfo" + myRaceChar + ".txt");
        int x, y, cnt;
        string line;
        while (readFileA) {
            readFileA >> x >> y >> cnt;

            for (auto &mineral : myMinerals) {

                if (x == mineral->getPosition().x && y == mineral->getPosition().y) {
                    while (cnt > 0) {
                        readFileA >> x >> y;
                        mineral->getGatherOrderPositions().insert(Position(x, y));
                        cnt--;
                    }
                }
            }
        }

        ifstream readFileB("bwapi-data/AI/" + mapName + "ReturnInfo" + myRaceChar + ".txt");
        while (readFileB) {
            readFileB >> x >> y >> cnt;

            for (auto &mineral : myMinerals) {

                if (x == mineral->getPosition().x && y == mineral->getPosition().y) {
                    while (cnt > 0) {
                        readFileB >> x >> y;
                        mineral->getReturnOrderPositions().insert(Position(x, y));
                        cnt--;
                    }
                }
            }
        }
    }

    void onEnd()
    {
        ofstream readFileA("bwapi-data/AI/" + mapName + "GatherInfo" + myRaceChar + ".txt");
        if (readFileA) {
            for (auto &mineral : myMinerals) {
                if (!mineral->getGatherOrderPositions().empty()) {
                    readFileA << mineral->getPosition().x << " " << mineral->getPosition().y << " " << mineral->getGatherOrderPositions().size() << "\n";
                    for (auto &pos : mineral->getGatherOrderPositions())
                        readFileA << pos.x << " " << pos.y << " ";
                    readFileA << "\n";
                }
            }
        }

        ofstream readFileB("bwapi-data/AI/" + mapName + "ReturnInfo" + myRaceChar + ".txt");
        if (readFileB) {
            for (auto &mineral : myMinerals) {
                if (!mineral->getReturnOrderPositions().empty()) {
                    readFileB << mineral->getPosition().x << " " << mineral->getPosition().y << " " << mineral->getReturnOrderPositions().size() << "\n";
                    for (auto &pos : mineral->getReturnOrderPositions())
                        readFileB << pos.x << " " << pos.y << " ";
                    readFileB << "\n";
                }
            }
        }
    }

    void storeResource(Unit resource)
    {
        auto info = ResourceInfo(resource);
        auto &resourceList = (!info.isBoulder() ? (resource->getType().isMineralField() ? myMinerals : myGas) : myBoulders);

        // Check if we already stored this resource
        for (auto &u : resourceList) {
            if (u->unit() == resource)
                return;
        }

        // Add station
        auto newStation = BWEB::Stations::getClosestStation(resource->getTilePosition());
        info.setStation(newStation);
        if (Stations::ownedBy(newStation) == PlayerState::Self)
            info.setResourceState(ResourceState::Assignable);
        resourceList.insert(make_shared<ResourceInfo>(info));
    }

    void removeResource(Unit unit)
    {
        auto &resource = getResourceInfo(unit);

        if (resource) {

            // Remove assignments
            for (auto &u : resource->targetedByWhat()) {
                if (!u.expired())
                    u.lock()->setResource(nullptr);
            }

            // Remove dead resources
            if (myMinerals.find(resource) != myMinerals.end())
                myMinerals.erase(resource);
            else if (myBoulders.find(resource) != myBoulders.end())
                myBoulders.erase(resource);
            else if (myGas.find(resource) != myGas.end())
                myGas.erase(resource);
        }
    }

    shared_ptr<ResourceInfo> getResourceInfo(BWAPI::Unit unit)
    {
        for (auto &m : myMinerals) {
            if (m->unit() == unit)
                return m;
        }
        for (auto &b : myBoulders) {
            if (b->unit() == unit)
                return b;
        }
        for (auto &g : myGas) {
            if (g->unit() == unit)
                return g;
        }
        return nullptr;
    }

    int getMineralCount() { return mineralCount; }
    int getGasCount() { return gasCount; }
    int getIncomeMineral() { return incomeMineral; }
    int getIncomeGas() { return incomeGas; }
    bool isMineralSaturated() { return mineralSat; }
    bool isHalfMineralSaturated() { return halfMineralSat; }
    bool isGasSaturated() { return gasSat; }
    bool isHalfGasSaturated() { return halfGasSat; }
    set<shared_ptr<ResourceInfo>>& getMyMinerals() { return myMinerals; }
    set<shared_ptr<ResourceInfo>>& getMyGas() { return myGas; }
    set<shared_ptr<ResourceInfo>>& getMyBoulders() { return myBoulders; }
}