#pragma once
#include <BWAPI.h>

namespace BWEB
{
    class Wall;

    class Path {
        std::vector<BWAPI::TilePosition> tiles;
        double dist;
        bool reachable, diagonal, cached;
        BWAPI::TilePosition source, target;
        BWAPI::UnitType type;
    public:
        Path(BWAPI::TilePosition _source, BWAPI::TilePosition _target, BWAPI::UnitType _type, bool _diagonal = true, bool _cached = true)
        {
            tiles ={};
            dist = 0.0;
            reachable = false;
            diagonal = _diagonal;
            cached = _cached;
            source = _source;
            target = _target;
            type = _type;
        }
        Path(BWAPI::Position _source, BWAPI::Position _target, BWAPI::UnitType _type, bool _diagonal = true, bool _cached = true)
        {
            tiles ={};
            dist = 0.0;
            reachable = false;
            diagonal = _diagonal;
            cached = _cached;
            source = BWAPI::TilePosition(_source);
            target = BWAPI::TilePosition(_target);
            type = _type;
        }
        Path()
        {
            tiles ={};
            dist = 0.0;
            reachable = false;
            diagonal = true;
            cached = true;
            source = BWAPI::TilePositions::Invalid;
            target = BWAPI::TilePositions::Invalid;
            type = BWAPI::UnitTypes::None;
        };

        /// <summary> Returns the vector of TilePositions associated with this Path. </summary>
        std::vector<BWAPI::TilePosition>& getTiles() { return tiles; }

        /// <summary> Returns the source (start) TilePosition of the Path. </summary>
        BWAPI::TilePosition getSource() { return source; }

        /// <summary> Returns the target (end) TilePosition of the Path. </summary>
        BWAPI::TilePosition getTarget() { return target; }

        /// <summary> Returns the distance from the source to the target in pixels. </summary>
        double getDistance() { return dist; }

        /// <summary> Returns a check if the path was able to reach the target. </summary>
        bool isReachable() { return reachable; }

        /// <summary> Creates a path from the source to the target using JPS with your provided walkable function. </summary>
        void generateJPS(std::function <bool(const BWAPI::TilePosition&)>);

        /// <summary> Creates a path from the source to the target using BFS with your provided walkable function. </summary>
        void generateBFS(std::function <bool(const BWAPI::TilePosition&)>);

        /// <summary> Creates a path from the source to the target using A* with your provided walkable function. </summary>
        void generateAS(std::function <double(const BWAPI::TilePosition&)>);

        /// <summary> Returns true if the TilePosition is walkable (does not include any buildings). </summary>
        bool terrainWalkable(const BWAPI::TilePosition &tile);

        /// <summary> Returns true if the TilePosition is walkable (includes buildings). </summary>
        bool unitWalkable(const BWAPI::TilePosition &tile);

        bool operator== (const Path& r) {
            return source == r.source && target == r.target;
        };

        bool operator!= (const Path& r) {
            return source != r.source || target != r.target;
        };
    };

    namespace Pathfinding {

        /// <summary> Clears the entire Pathfinding cache. All Paths will be generated as a new Path. </summary>
        void clearCacheFully();

        /// <summary> Clears the Pathfinding cache for a specific walkable function. All Paths will be generated as a new Path. </summary>
        void clearCache(std::function <bool(const BWAPI::TilePosition&)>);

        void testCache();
    }
}
