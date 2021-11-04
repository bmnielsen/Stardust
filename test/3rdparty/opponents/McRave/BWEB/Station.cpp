#include "BWEB.h"

using namespace std;
using namespace BWAPI;

namespace BWEB {

    namespace {
        vector<Station> stations;
        vector<const BWEM::Base *> mainBases;
        vector<const BWEM::Base *> natBases;
    }

    void Station::addResourceReserves()
    {
        const auto addReserve = [&](Unit resource) {
            TilePosition start(resource->getPosition());
            vector<TilePosition> directions{ {1,0}, {-1,0}, {0, 1}, {0,-1} };
            auto end = (base->Center() * 5 / 6) + (resourceCentroid / 6);

            // Get the starting tile
            auto distClosest = DBL_MAX;
            for (int x = resource->getTilePosition().x; x < resource->getTilePosition().x + resource->getType().tileWidth(); x++) {
                for (int y = resource->getTilePosition().y; y < resource->getTilePosition().y + resource->getType().tileHeight(); y++) {
                    auto tile = TilePosition(x, y);
                    auto center = Position(tile) + Position(16, 16);
                    auto dist = center.getDistance(resourceCentroid);
                    if (dist < distClosest) {
                        start = tile;
                        distClosest = dist;
                    }
                }
            }

            TilePosition next = start;
            while (next != TilePosition(end)) {
                auto distBest = DBL_MAX;
                start = next;
                for (auto &t : directions) {
                    auto tile = start + t;
                    auto pos = Position(tile) + Position(16, 16);

                    if (!tile.isValid())
                        continue;

                    auto dist = pos.getDistance(end);
                    if (dist <= distBest) {
                        next = tile;
                        distBest = dist;
                    }
                }

                if (next.isValid()) {
                    Map::addReserve(next, 1, 1);

                    // Remove any defenses in the way of a geyser
                    if (!resource->getType().isMineralField()) {
                        for (auto &def : defenses) {
                            if (next.x >= def.x && next.x < def.x + 2 && next.y >= def.y && next.y < def.y + 2) {
                                defenses.erase(def);
                                break;
                            }
                        }
                    }
                }
            }
        };

        // Add reserved tiles
        for (auto &m : base->Minerals()) {
            Map::addReserve(m->TopLeft(), 2, 1);
            addReserve(m->Unit());
        }
        for (auto &g : base->Geysers()) {
            Map::addReserve(g->TopLeft(), 4, 2);
            addReserve(g->Unit());
        }
    }

    void Station::initialize()
    {
        auto cnt = 0;

        // Resource and defense centroids
        for (auto &mineral : base->Minerals()) {
            resourceCentroid += mineral->Pos();
            cnt++;
        }

        if (cnt > 0)
            defenseCentroid = resourceCentroid / cnt;

        for (auto &gas : base->Geysers()) {
            defenseCentroid = (defenseCentroid + gas->Pos()) / 2;
            resourceCentroid += gas->Pos();
            cnt++;
        }

        if (cnt > 0)
            resourceCentroid = resourceCentroid / cnt;
        Map::addUsed(base->Location(), Broodwar->self()->getRace().getResourceDepot());
    }

    void Station::findChoke()
    {
        // Only find a Chokepoint for mains or naturals
        if (!main && !natural)
            return;

        // Get closest partner base
        auto distBest = DBL_MAX;
        for (auto &potentialPartner : (main ? natBases : mainBases)) {
            auto dist = potentialPartner->Center().getDistance(base->Center());
            if (dist < distBest) {
                partnerBase = potentialPartner;
                distBest = dist;
            }
        }
        if (!partnerBase)
            return;


        if (main && !Map::mapBWEM.GetPath(partnerBase->Center(), base->Center()).empty()) {
            choke = Map::mapBWEM.GetPath(partnerBase->Center(), base->Center()).front();

            // Partner only has one chokepoint means we have a shared choke with this path
            if (partnerBase->GetArea()->ChokePoints().size() == 1)
                choke = Map::mapBWEM.GetPath(partnerBase->Center(), base->Center()).back();
        }

        else {

            // Only one chokepoint in this area
            if (base->GetArea()->ChokePoints().size() == 1) {
                choke = base->GetArea()->ChokePoints().front();
                return;
            }

            set<BWEM::ChokePoint const *> nonChokes;
            for (auto &choke : Map::mapBWEM.GetPath(partnerBase->Center(), base->Center()))
                nonChokes.insert(choke);

            auto distBest = DBL_MAX;
            const BWEM::Area* second = nullptr;

            // Iterate each neighboring area to get closest to this natural area
            for (auto &area : base->GetArea()->AccessibleNeighbours()) {
                auto center = area->Top();
                const auto dist = Position(center).getDistance(Map::mapBWEM.Center());

                bool wrongArea = false;
                for (auto &choke : area->ChokePoints()) {
                    if ((!choke->Blocked() && choke->Pos(choke->end1).getDistance(choke->Pos(choke->end2)) <= 2) || nonChokes.find(choke) != nonChokes.end()) {
                        wrongArea = true;
                    }
                }
                if (wrongArea)
                    continue;

                if (center.isValid() && dist < distBest) {
                    second = area;
                    distBest = dist;
                }
            }

            distBest = DBL_MAX;
            for (auto &c : base->GetArea()->ChokePoints()) {
                if (c->Center() == BWEB::Map::getMainChoke()->Center()
                    || c->Blocked()
                    || c->Geometry().size() <= 3
                    || (c->GetAreas().first != second && c->GetAreas().second != second))
                    continue;

                const auto dist = Position(c->Center()).getDistance(Position(partnerBase->Center()));
                if (dist < distBest) {
                    choke = c;
                    distBest = dist;
                }
            }
        }

        if (choke && !main)
            defenseCentroid = Position(choke->Center());

    }

    void Station::findSecondaryLocations()
    {
        if (Broodwar->self()->getRace() != Races::Zerg)
            return;

        auto cnt = 0;
        if (main)
            cnt = 1;
        if (!main && !natural)
            cnt = 2;

        for (int i = 0; i < cnt; i++) {
            auto distBest = DBL_MAX;
            auto tileBest = TilePositions::Invalid;
            for (auto x = base->Location().x - 4; x <= base->Location().x + 4; x++) {
                for (auto y = base->Location().y - 3; y <= base->Location().y + 3; y++) {
                    auto tile = TilePosition(x, y);
                    auto center = Position(tile) + Position(64, 48);
                    auto dist = center.getDistance(resourceCentroid);
                    if (dist < distBest && Map::isPlaceable(Broodwar->self()->getRace().getResourceDepot(), tile)) {
                        distBest = dist;
                        tileBest = tile;
                    }
                }
            }

            if (tileBest.isValid()) {
                secondaryLocations.insert(tileBest);
                Map::addUsed(tileBest, Broodwar->self()->getRace().getResourceDepot());
            }
        }
    }

    void Station::findDefenses()
    {
        vector<TilePosition> basePlacements;
        vector<TilePosition> geyserPlacements ={ {-2, -2}, {-2, 0}, {-2, 2}, {0, -2}, {0, 2}, {2, -2}, {2, 2}, {4, -2}, {4, 0}, {4, 2} };
        auto here = base->Location();
        auto defenseType = UnitTypes::None;
        if (Broodwar->self()->getRace() == Races::Protoss)
            defenseType = UnitTypes::Protoss_Photon_Cannon;
        if (Broodwar->self()->getRace() == Races::Terran)
            defenseType = UnitTypes::Terran_Missile_Turret;
        if (Broodwar->self()->getRace() == Races::Zerg)
            defenseType = UnitTypes::Zerg_Creep_Colony;

        // Get angle of chokepoint
        if (choke && base && (main || natural)) {
            auto dist = min(480.0, getBase()->Center().getDistance(Position(choke->Center())));
            baseAngle = fmod(Map::getAngle(make_pair(getBase()->Center(), Position(choke->Center()) + Position(4, 4))), 3.14);
            chokeAngle = fmod(Map::getAngle(make_pair(Position(choke->Pos(choke->end1)), Position(choke->Pos(choke->end2)))), 3.14);

            auto diff = baseAngle - chokeAngle;
            diff > 0.7 ? baseAngle -= 1.57 : baseAngle += 1.57;
            defenseAngle = max(0.0, (baseAngle  * (dist / 480.0)) + (chokeAngle * (480.0 - dist) / 480.0));

            if (base->GetArea()->ChokePoints().size() >= 3) {
                const BWEM::ChokePoint * validSecondChoke = nullptr;
                for (auto &otherChoke : base->GetArea()->ChokePoints()) {
                    if (choke == otherChoke)
                        continue;

                    if ((choke->GetAreas().first == otherChoke->GetAreas().first && choke->GetAreas().second == otherChoke->GetAreas().second)
                        || (choke->GetAreas().first == otherChoke->GetAreas().second && choke->GetAreas().second == otherChoke->GetAreas().first))
                        validSecondChoke = otherChoke;
                }

                if (validSecondChoke)
                    defenseAngle = (Map::getAngle(make_pair(Position(choke->Center()), Position(validSecondChoke->Center()))) + Map::getAngle(make_pair(Position(choke->Pos(choke->end1)), Position(choke->Pos(choke->end2))))) / 2.0;
            }
        }
        else {
            defenseCentroid = BWEB::Map::mapBWEM.Center();
            defenseAngle = fmod(Map::getAngle(make_pair(Position(getBase()->Center()), defenseCentroid)), 3.14) + 1.57;
        }

        // Round to nearest pi/8 rads
        auto nearestEight = int(round(defenseAngle / 0.3926991));
        auto angle = nearestEight % 8;

        // Generate defenses
        if (main)
            basePlacements ={ {-2, -2}, {-2, 1}, {1, -2} };
        else {
            if (angle == 0)
                basePlacements ={ {-2, 2}, {-2, 0}, {-2, -2}, {0, 3}, {0, -2}, {2, -2}, {4, -2}, {4, 0}, {4, 2} };   // 0/8                
            if (angle == 1 || angle == 7)
                basePlacements ={ {-2, 3}, {-2, 1}, {-2, -1}, {0, -2}, {1, 3}, {2, -2}, {4, -1}, {4, 1}, };  // pi/8                
            if (angle == 2 || angle == 6)
                basePlacements ={ {-2, 2}, {-2, 0}, {0, 3}, {0, -2}, {2, -2}, {4, -2}, {4, 0} };   // pi/4                
            if (angle == 3 || angle == 5)
                basePlacements ={ {-2, 2}, {-2, 0}, {-1, -2}, {0, 3}, {1, -2}, {2, 3}, {3, -2}, {4, 0} };  // 3pi/8                
            if (angle == 4)
                basePlacements ={ {-2, 2}, {-2, 0}, {-2, -2}, {0, 3}, {0, -2}, {2, 3}, {2, -2}, {4, 3}, {4, -2} };   // pi/2
        }

        // Flip them vertically / horizontally as needed
        if (base->Center().y < defenseCentroid.y) {
            for (auto &placement : basePlacements)
                placement.y = -(placement.y - 1);
        }
        if (base->Center().x < defenseCentroid.x) {
            for (auto &placement : basePlacements)
                placement.x = -(placement.x - 2);
        }

        // Add scanner addon for Terran
        if (Broodwar->self()->getRace() == Races::Terran) {
            auto scannerTile = here + TilePosition(4, 1);
            defenses.insert(scannerTile);
            Map::addUsed(scannerTile, defenseType);
        }

        // Add a defense near each base placement if possible
        for (auto &placement : basePlacements) {
            auto tile = base->Location() + placement;
            if (Map::isPlaceable(defenseType, tile)) {
                defenses.insert(tile);
                Map::addUsed(tile, defenseType);
            }
        }

        // Add geyser defenses
        if (main) {
            for (auto &geyser : base->Geysers()) {
                for (auto &placement : geyserPlacements) {
                    auto tile = geyser->TopLeft() + placement;
                    auto center = Position(tile) + Position(16, 16);
                    if (center.getDistance(base->Center()) > geyser->Pos().getDistance(base->Center()) && Map::isPlaceable(defenseType, tile)) {
                        defenses.insert(tile);
                        Map::addUsed(tile, defenseType);
                    }
                }
            }
        }

    }

    void Station::draw()
    {
        int color = Broodwar->self()->getColor();
        int textColor = color == 185 ? textColor = Text::DarkGreen : Broodwar->self()->getTextColor();

        // Draw boxes around each feature
        for (auto &tile : defenses) {
            Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(65, 65), color);
            Broodwar->drawTextMap(Position(tile) + Position(4, 52), "%cS", textColor);
        }

        // Draw corresponding choke
        if (choke && base && (main || natural)) {

            if (base->GetArea()->ChokePoints().size() >= 3) {
                const BWEM::ChokePoint * validSecondChoke = nullptr;
                for (auto &otherChoke : base->GetArea()->ChokePoints()) {
                    if (choke == otherChoke)
                        continue;

                    if ((choke->GetAreas().first == otherChoke->GetAreas().first && choke->GetAreas().second == otherChoke->GetAreas().second)
                        || (choke->GetAreas().first == otherChoke->GetAreas().second && choke->GetAreas().second == otherChoke->GetAreas().first))
                        validSecondChoke = choke;
                }

                if (validSecondChoke) {
                    Broodwar->drawLineMap(Position(validSecondChoke->Pos(validSecondChoke->end1)), Position(validSecondChoke->Pos(validSecondChoke->end2)), Colors::Grey);
                    Broodwar->drawLineMap(base->Center(), Position(validSecondChoke->Center()), Colors::Grey);
                    Broodwar->drawLineMap(Position(choke->Center()), Position(validSecondChoke->Center()), Colors::Grey);
                }
            }

            Broodwar->drawLineMap(Position(choke->Pos(choke->end1)), Position(choke->Pos(choke->end2)), Colors::Grey);
            Broodwar->drawLineMap(base->Center(), Position(choke->Center()), Colors::Grey);
        }

        // Label angle
        Broodwar->drawTextMap(base->Center() - Position(0, 16), "%c%.2f", Text::White, baseAngle);
        Broodwar->drawTextMap(base->Center(), "%c%.2f", Text::White, chokeAngle);
        Broodwar->drawTextMap(base->Center() + Position(0, 16), "%c%.2f", Text::White, defenseAngle);

        Broodwar->drawBoxMap(Position(base->Location()), Position(base->Location()) + Position(129, 97), color);
        Broodwar->drawTextMap(Position(base->Location()) + Position(4, 84), "%cS", textColor);
        for (auto &location : secondaryLocations) {
            Broodwar->drawBoxMap(Position(location), Position(location) + Position(129, 97), color);
            Broodwar->drawTextMap(Position(location) + Position(4, 84), "%cS", textColor);
        }
    }

    void Station::cleanup()
    {
        // Remove used on defenses
        for (auto &tile : defenses) {
            Map::removeUsed(tile, 2, 2);
            Map::addReserve(tile, 2, 2);
        }

        // Remove used on secondary locations
        for (auto &tile : secondaryLocations) {
            Map::removeUsed(tile, 4, 3);
            Map::addReserve(tile, 4, 3);
        }

        // Remove used on main location
        Map::removeUsed(getBase()->Location(), 4, 3);
        Map::addReserve(getBase()->Location(), 4, 3);
    }
}

namespace BWEB::Stations {

    void findStations()
    {
        // Find all main bases
        for (auto &area : Map::mapBWEM.Areas()) {
            for (auto &base : area.Bases()) {
                if (base.Starting())
                    mainBases.push_back(&base);
            }
        }

        // Find all natural bases        
        for (auto &main : mainBases) {

            const BWEM::Base * baseBest = nullptr;
            auto distBest = DBL_MAX;
            for (auto &area : Map::mapBWEM.Areas()) {
                for (auto &base : area.Bases()) {

                    // Must have gas, be accesible and at least 5 mineral patches
                    if (base.Starting()
                        || base.Geysers().empty()
                        || base.GetArea()->AccessibleNeighbours().empty()
                        || base.Minerals().size() < 5)
                        continue;

                    const auto dist = Map::getGroundDistance(base.Center(), main->Center());
                    if (dist < distBest) {
                        distBest = dist;
                        baseBest = &base;
                    }
                }
            }

            // Store any natural we found
            if (baseBest)
                natBases.push_back(baseBest);
        }

        // Create Stations
        for (auto &area : Map::mapBWEM.Areas()) {
            for (auto &base : area.Bases()) {

                auto isMain = find(mainBases.begin(), mainBases.end(), &base) != mainBases.end();
                auto isNatural = find(natBases.begin(), natBases.end(), &base) != natBases.end();

                // Add to our station lists
                Station newStation(&base, isMain, isNatural);
                stations.push_back(newStation);
            }
        }
    }

    void draw()
    {
        for (auto &station : Stations::getStations())
            station.draw();
    }

    Station * getClosestStation(TilePosition here)
    {
        auto distBest = DBL_MAX;
        Station* bestStation = nullptr;
        for (auto &station : stations) {
            const auto dist = here.getDistance(station.getBase()->Location());

            if (dist < distBest) {
                distBest = dist;
                bestStation = &station;
            }
        }
        return bestStation;
    }

    Station * getClosestMainStation(TilePosition here)
    {
        auto distBest = DBL_MAX;
        Station * bestStation = nullptr;
        for (auto &station : stations) {
            if (!station.isMain())
                continue;
            const auto dist = here.getDistance(station.getBase()->Location());

            if (dist < distBest) {
                distBest = dist;
                bestStation = &station;
            }
        }
        return bestStation;
    }

    Station * getClosestNaturalStation(TilePosition here)
    {
        auto distBest = DBL_MAX;
        Station* bestStation = nullptr;
        for (auto &station : stations) {
            if (!station.isNatural())
                continue;
            const auto dist = here.getDistance(station.getBase()->Location());

            if (dist < distBest) {
                distBest = dist;
                bestStation = &station;
            }
        }
        return bestStation;
    }

    vector<Station>& getStations() {
        return stations;
    }
}