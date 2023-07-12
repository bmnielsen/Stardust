//////////////////////////////////////////////////////////////////////////
//
// This file is part of the BWEM Library.
// BWEM is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2015, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////

#include "area.h"

#include <map>
#include <memory>

#include "mapImpl.h"
#include "graph.h"
#include "neutral.h"
#include "winutils.h"
#include "bwapiExt.h"

#include "BaseFinder\BaseFinder.h"

using namespace BWAPI;
using namespace BWAPI::UnitTypes::Enum;
namespace { auto & bw = Broodwar; }

using namespace std;


namespace BWEM {

    using namespace detail;
    using namespace BWAPI_ext;



    //////////////////////////////////////////////////////////////////////////////////////////////
    //                                                                                          //
    //                                  class Area
    //                                                                                          //
    //////////////////////////////////////////////////////////////////////////////////////////////


    Area::Area(Graph * pGraph, id areaId, WalkPosition top, int miniTiles)
        : m_pGraph(pGraph), m_id(areaId), m_top(top), m_miniTiles(miniTiles)
    {
        bwem_assert(areaId > 0);

        auto & topMiniTile = GetMap()->GetMiniTile(top);
        bwem_assert(topMiniTile.AreaId() == areaId);

        m_maxAltitude = topMiniTile.Altitude();
    }


    Area::Area(const Area & Other)
        : m_pGraph(Other.m_pGraph)
    {
        bwem_assert(false);
    }


    Map * Area::GetMap() const
    {
        return m_pGraph->GetMap();
    }


    TilePosition Area::BoundingBoxSize() const
    {
        return m_bottomRight - m_topLeft + 1;
    }


    const std::vector<ChokePoint> & Area::ChokePoints(const Area * pArea) const
    {
        auto it = m_ChokePointsByArea.find(pArea);
        bwem_assert(it != m_ChokePointsByArea.end());
        return *it->second;
    }


    void Area::AddGeyser(Geyser * pGeyser)
    {
        bwem_assert(pGeyser && !contains(m_Geysers, pGeyser));
        m_Geysers.push_back(pGeyser);
    }


    void Area::AddMineral(Mineral * pMineral)
    {
        bwem_assert(pMineral && !contains(m_Minerals, pMineral));
        m_Minerals.push_back(pMineral);
    }


    void Area::OnMineralDestroyed(const Mineral * pMineral)
    {
        bwem_assert(pMineral);

        auto iMineral = find(m_Minerals.begin(), m_Minerals.end(), pMineral);
        if (iMineral != m_Minerals.end())
            fast_erase(m_Minerals, distance(m_Minerals.begin(), iMineral));

        // let's examine the bases even if pMineral was not found in this Area,
        // which could arise if Minerals were allowed to be assigned to neighbouring Areas.
        for (Base & base : Bases())
            base.OnMineralDestroyed(pMineral);
    }


    void Area::AddChokePoints(Area * pArea, vector<ChokePoint> * pChokePoints)
    {
        bwem_assert(!m_ChokePointsByArea[pArea] && pChokePoints);

        m_ChokePointsByArea[pArea] = pChokePoints;

        for (const auto & cp : *pChokePoints)
            m_ChokePoints.push_back(&cp);
    }



    vector<int> Area::ComputeDistances(const ChokePoint * pStartCP, const vector<const ChokePoint *> & TargetCPs) const
    {
        bwem_assert(!contains(TargetCPs, pStartCP));

        TilePosition start = GetMap()->BreadthFirstSearch(TilePosition(pStartCP->PosInArea(ChokePoint::middle, this)),
            [this](const Tile & tile, TilePosition) { return tile.AreaId() == Id(); },    // findCond
            [](const Tile &, TilePosition) { return true; });                    // visitCond

        vector<TilePosition> Targets;
        for (const ChokePoint * cp : TargetCPs)
            Targets.push_back(GetMap()->BreadthFirstSearch(TilePosition(cp->PosInArea(ChokePoint::middle, this)),
                [this](const Tile & tile, TilePosition) { return tile.AreaId() == Id(); },    // findCond
                [](const Tile &, TilePosition) { return true; }));                    // visitCond

        return ComputeDistances(start, Targets);
    }


    // Returns Distances such that Distances[i] == ground_distance(start, Targets[i]) in pixels
    // Note: same algorithm than Graph::ComputeDistances (derived from Dijkstra)
    vector<int> Area::ComputeDistances(TilePosition start, const vector<TilePosition> & Targets) const
    {
        const Map * pMap = GetMap();
        vector<int> Distances(Targets.size());

        Tile::UnmarkAll();

        multimap<int, TilePosition> ToVisit;    // a priority queue holding the tiles to visit ordered by their distance to start.
        ToVisit.emplace(0, start);

        int remainingTargets = Targets.size();
        while (!ToVisit.empty())
        {
            int currentDist = ToVisit.begin()->first;
            TilePosition current = ToVisit.begin()->second;
            const Tile & currentTile = pMap->GetTile(current, check_t::no_check);
            bwem_assert(currentTile.InternalData() == currentDist);
            ToVisit.erase(ToVisit.begin());
            currentTile.SetInternalData(0);                                        // resets Tile::m_internalData for future usage
            currentTile.SetMarked();

            for (int i = 0; i < (int)Targets.size(); ++i)
                if (current == Targets[i])
                {
                    Distances[i] = int(0.5 + currentDist * 32 / 10000.0);
                    --remainingTargets;
                }
            if (!remainingTargets) break;

            for (TilePosition delta : {    TilePosition(-1, -1), TilePosition(0, -1), TilePosition(+1, -1),
                TilePosition(-1, 0), TilePosition(+1, 0),
                TilePosition(-1, +1), TilePosition(0, +1), TilePosition(+1, +1)})
            {
                const bool diagonalMove = (delta.x != 0) && (delta.y != 0);
                const int newNextDist = currentDist + (diagonalMove ? 14142 : 10000);

                TilePosition next = current + delta;
                if (pMap->Valid(next))
                {
                    const Tile & nextTile = pMap->GetTile(next, check_t::no_check);
                    if (!nextTile.Marked())
                    {
                        if (nextTile.InternalData())    // next already in ToVisit
                        {
                            if (newNextDist < nextTile.InternalData())        // nextNewDist < nextOldDist
                            {    // To update next's distance, we need to remove-insert it from ToVisit:
                                auto range = ToVisit.equal_range(nextTile.InternalData());
                                auto iNext = find_if(range.first, range.second, [next]
                                (const pair<int, TilePosition> & e) { return e.second == next; });
                                bwem_assert(iNext != range.second);

                                ToVisit.erase(iNext);
                                nextTile.SetInternalData(newNextDist);
                                //    nextTile.SetPtr(const_cast<Tile *>(&currentTile));        // note: we won't use this backward trace
                                ToVisit.emplace(newNextDist, next);
                            }
                        }
                        else if ((nextTile.AreaId() == Id()) || (nextTile.AreaId() == -1))
                        {
                            nextTile.SetInternalData(newNextDist);
                            //    nextTile.SetPtr(const_cast<Tile *>(&currentTile));            // note: we won't use this backward trace
                            ToVisit.emplace(newNextDist, next);
                        }
                    }
                }
            }
        }

        bwem_assert(!remainingTargets);

        // Reset Tile::m_internalData for future usage
        for (auto e : ToVisit)
            pMap->GetTile(e.second, check_t::no_check).SetInternalData(0);

        return Distances;
    }


    void Area::UpdateAccessibleNeighbours()
    {
        m_AccessibleNeighbours.clear();

        for (auto it : ChokePointsByArea())
            if (any_of(it.second->begin(), it.second->end(), [](const ChokePoint & cp) { return !cp.Blocked(); }))
                m_AccessibleNeighbours.push_back(it.first);
    }


    // Called for each tile t of this Area
    void Area::AddTileInformation(const BWAPI::TilePosition t, const Tile & tile)
    {
        ++m_tiles;
        if (tile.Buildable()) ++m_buildableTiles;
        if (tile.GroundHeight() == 1) ++m_highGroundTiles;
        if (tile.GroundHeight() == 2) ++m_veryHighGroundTiles;

        if (t.x < m_topLeft.x)     m_topLeft.x = t.x;
        if (t.y < m_topLeft.y)     m_topLeft.y = t.y;
        if (t.x > m_bottomRight.x) m_bottomRight.x = t.x;
        if (t.y > m_bottomRight.y) m_bottomRight.y = t.y;
    }


    // Called after AddTileInformation(t) has been called for each tile t of this Area
    void Area::PostCollectInformation()
    {
    }



    // Calculates the score >= 0 corresponding to the placement of a Base Command Center at 'location'.
    // The more there are ressources nearby, the higher the score is.
    // The function assumes the distance to the nearby ressources has already been computed (in InternalData()) for each tile around.
    // The job is therefore made easier : just need to sum the InternalData() values.
    // Returns -1 if the location is impossible.

    int Area::ComputeBaseLocationScore(TilePosition location) const
    {
        const Map * pMap = GetMap();
        const TilePosition dimCC = UnitType(Terran_Command_Center).tileSize();

        int sumScore = 0;
        for (int dy = 0; dy < dimCC.y; ++dy)
            for (int dx = 0; dx < dimCC.x; ++dx)
            {
                const Tile & tile = pMap->GetTile(location + TilePosition(dx, dy), check_t::no_check);
                if (!tile.Buildable()) return -1;
                if (tile.InternalData() == -1) return -1;        // The special value InternalData() == -1 means there is some ressource at maximum 3 tiles, which Starcraft rules forbid.
                                                        // Unfortunately, this is guaranteed only for the ressources in this Area, which is the very reason of ValidateBaseLocation
                if (tile.AreaId() != Id()) return -1;
                if (tile.GetNeutral() && tile.GetNeutral()->IsStaticBuilding()) return -1;

                sumScore += tile.InternalData();
            }

        return sumScore;
    }


    // Checks if 'location' is a valid location for the placement of a Base Command Center.
    // If the location is valid except for the presence of Mineral patches of less than 9 (see Andromeda.scx),
    // the function returns true, and these Minerals are reported in BlockingMinerals
    // The function is intended to be called after ComputeBaseLocationScore, as it is more expensive.
    // See also the comments inside ComputeBaseLocationScore.
    bool Area::ValidateBaseLocation(TilePosition location, vector<Mineral *> & BlockingMinerals) const
    {
        const Map * pMap = GetMap();
        const TilePosition dimCC = UnitType(Terran_Command_Center).tileSize();

        BlockingMinerals.clear();

        for (int dy = -3; dy < dimCC.y + 3; ++dy)
            for (int dx = -3; dx < dimCC.x + 3; ++dx)
            {
                TilePosition t = location + TilePosition(dx, dy);
                if (pMap->Valid(t))
                {
                    const Tile & tile = pMap->GetTile(t, check_t::no_check);
                    if (Neutral * n = tile.GetNeutral())
                    {
                        if (n->IsGeyser()) return false;
                        if (Mineral * m = n->IsMineral())
                            if (m->InitialAmount() <= 8) BlockingMinerals.push_back(m);
                            else return false;
                    }
                }
            }

        // checks the distance to the Bases already created:
        for (const Base & base : Bases())
            if (roundedDist(base.Location(), location) < min_tiles_between_Bases) return false;

        return true;
    }


    void Area::CreateBases()
    {
        const Map * pMap = GetMap();
        m_Bases.reserve(min(100, (int)pMap->Minerals().size() + (int)pMap->Geysers().size()));

        // Look through all the bases we found
        for (const auto &base : BaseFinder::GetBases()) {

            if (pMap->GetArea(BWAPI::TilePosition(base.pos)) == this) {
                vector<Ressource *> assignedResources;
                vector<Mineral *> blockingMinerals;

                // Look through the resources on the map and assign to the base if the base tracks this resource
                for (const auto &bwemMineral : pMap->Minerals()) {
                    if (bwemMineral->InitialAmount() > 40 && find_if(base.minerals.begin(), base.minerals.end(), [&](auto &u) { return u->getTilePosition() == bwemMineral->TopLeft(); }) != base.minerals.end())
                        assignedResources.push_back(bwemMineral.get());
                }
                for (const auto &bwemGas : pMap->Geysers()) {
                    if (find_if(base.geysers.begin(), base.geysers.end(), [&](auto &u) { return u->getTilePosition() == bwemGas->TopLeft(); }) != base.geysers.end())
                        assignedResources.push_back(bwemGas.get());
                }

                // Add in any blocking minerals
                for (int y = base.tpos.y; y < base.tpos.y + 3; ++y) {
                    for (int x = base.tpos.x; x < base.tpos.x + 4; ++x)
                        ValidateBaseLocation(TilePosition(x, y), blockingMinerals);
                }
                m_Bases.emplace_back(this, base.tpos, assignedResources, blockingMinerals);
            }
        }
    }

} // namespace BWEM



