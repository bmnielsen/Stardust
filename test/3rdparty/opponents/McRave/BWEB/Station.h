#pragma once
#include <set>
#include <BWAPI.h>
#include <bwem.h>

namespace BWEB {

    class Station
    {
        const BWEM::Base* base = nullptr;
        const BWEM::Base * partnerBase = nullptr;
        const BWEM::ChokePoint* choke = nullptr;
        BWAPI::Position resourceCentroid, defenseCentroid;
        std::set<BWAPI::TilePosition> secondaryLocations;
        std::set<BWAPI::TilePosition> defenses;
        bool main, natural;
        double defenseAngle = 0.0;
        double baseAngle = 0.0;
        double chokeAngle = 0.0;

        void initialize();
        void findChoke();
        void findSecondaryLocations();
        void findDefenses();
        void addResourceReserves();
        void cleanup();

    public:
        Station(const BWEM::Base* _base, bool _main, bool _natural)
        {
            base                = _base;
            main                = _main;
            natural             = _natural;
            resourceCentroid    = BWAPI::Position(0, 0);
            defenseCentroid     = BWAPI::Position(0, 0);
            initialize();
            findChoke();
            findSecondaryLocations();
            findDefenses();
            addResourceReserves();
            cleanup();
        }

        /// <summary> Returns the central position of the resources associated with this Station including geysers. </summary>
        BWAPI::Position getResourceCentroid() { return resourceCentroid; }

        /// <summary> Returns a backup base placement in case the main is blocked or a second one is desired for walling purposes. </summary>
        std::set<BWAPI::TilePosition> getSecondaryLocations() { return secondaryLocations; }

        /// <summary> Returns the set of defense locations associated with this Station. </summary>
        std::set<BWAPI::TilePosition>& getDefenses() { return defenses; }

        /// <summary> Returns the BWEM Base associated with this Station. </summary>
        const BWEM::Base * getBase() { return base; }

        /// <summary> Returns the BWEM Choke that should be used for generating a Wall at. </summmary>
        const BWEM::ChokePoint * getChokepoint() { return choke; }

        /// <summary> Returns true if the Station is a main Station. </summary>
        bool isMain() { return main; }

        /// <summary> Returns true if the Station is a natural Station. </summary>
        bool isNatural() { return natural; }

        /// <summary> Draws all the features of the Station. </summary>
        void draw();

        ///
        double getDefenseAngle() { return defenseAngle; }

        bool operator== (Station* s) {
            return s && base == s->getBase();
        }
        bool operator== (Station s) {
            return base == s.getBase();
        }
    };

    namespace Stations {

        /// <summary> Returns a vector containing every BWEB::Station </summary>
        std::vector<Station>& getStations();

        /// <summary> Returns the closest BWEB::Station to the given TilePosition. </summary>
        Station * getClosestStation(BWAPI::TilePosition);

        /// <summary> Returns the closest main BWEB::Station to the given TilePosition. </summary>
        Station * getClosestMainStation(BWAPI::TilePosition);

        /// <summary> Returns the closest natural BWEB::Station to the given TilePosition. </summary>
        Station * getClosestNaturalStation(BWAPI::TilePosition);

        /// <summary> Initializes the building of every BWEB::Station on the map, call it only once per game. </summary>
        void findStations();

        /// <summary> Calls the draw function for each Station that exists. </summary>
        void draw();
    }
}