//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "constructing.h"
#include "fleeing.h"
#include "walking.h"
#include "defaultBehavior.h"
#include "../units/cc.h"
#include "../units/turret.h"
#include "../territory/stronghold.h"
#include "../territory/vgridmap.h"
#include "../territory/vbase.h"
#include "../territory/wall.h"
#include "../strategy/strategy.h"
#include "../strategy/walling.h"
#include "../strategy/firstFactoryPlacement.h"
#include "../strategy/firstBarracksPlacement.h"
#include "../strategy/firstDepotPlacement.h"
#include "../strategy/secondDepotPlacement.h"
#include "../strategy/cannonRush.h"
#include "../strategy/marineRush.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/expand.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


int beingConstructed(BWAPI::UnitType type)
{
	return count_if(Constructing::Instances().begin(), Constructing::Instances().end(), [type]
		(const Constructing * p){ return p->Type() == type; });
}



static int countVerticalAdjacentBuildings(TilePosition pos, TilePosition dim, int yDir)
{
	assert_(abs(yDir) == 1);

	TilePosition t = pos + TilePosition(0, yDir*dim.y);
	if (ai()->GetMap().Valid(t))
		if (const BWAPIUnit * u = ai()->GetVMap().GetBuildingOn(ai()->GetMap().GetTile(t)))
			if (const MyBuilding * b = u->IsMyBuilding())
				if ((b->TopLeft() == t) && (b->Size() == dim))
					return 1 + countVerticalAdjacentBuildings(t, dim, yDir);
		
	return 0;
}


bool rawCanBuild(BWAPI::UnitType type, TilePosition location)
{
	TilePosition dim(type.tileSize());
//	if (type.canBuildAddon()) dim.x += 2;

	if (!ai()->GetMap().Valid(location)) return false;
	if (!ai()->GetMap().Valid(location + dim - 1)) return false;

	for (int dy = 0 ; dy < dim.y ; ++dy)
	for (int dx = 0 ; dx < dim.x ; ++dx)
	{
		TilePosition t = location + TilePosition(dx, dy);

		const auto & tile = ai()->GetMap().GetTile(t);
		
		if (!tile.Buildable()) return false;
		if (tile.GetNeutral()) return false;

		if (type == Terran_Command_Center)
		{
			if (ai()->GetVMap().CommandCenterImpossible(tile)) return false;
		}
		else
		{
//			if (ai()->GetVMap().CommandCenterWithAddonRoom(tile)) return false;
		}
	}

	return true;
}



static bool canBuild(BWAPI::UnitType type, TilePosition location, int & verticalAdjacentBuildings)
{
	TilePosition dim(type.tileSize());
	if (type.canBuildAddon()) dim.x += 2;

	verticalAdjacentBuildings = 0;
	int topAdjacentBuildings = 0;		// clear warning
	int bottomAdjacentBuildings = 0;	// clear warning

	CHECK_POS(location);
	CHECK_POS(location + dim - 1);

	for (int dy = -1 ; dy <= dim.y ; ++dy)
	for (int dx = -1 ; dx <= dim.x ; ++dx)
	{
		TilePosition t = location + TilePosition(dx, dy);

		if (dy == -1)
		{
			if (dx == -1)
				topAdjacentBuildings = countVerticalAdjacentBuildings(location, type.tileSize(), -1);

			if ((topAdjacentBuildings == 1) || (topAdjacentBuildings == 2)) continue;
		}
		else if (dy == dim.y)
		{
			if (dx == -1)
				bottomAdjacentBuildings = countVerticalAdjacentBuildings(location, type.tileSize(), +1);

			if ((bottomAdjacentBuildings >= 1) && (topAdjacentBuildings + bottomAdjacentBuildings <= 2)) continue;
		}

		bool border = (dx == -1) || (dy == -1) || (dx == dim.x) || (dy == dim.y);
		if (border)
			if (!ai()->GetMap().Valid(t))
				if (type == Terran_Missile_Turret) return false;
				else continue;

		const auto & tile = ai()->GetMap().GetTile(t);
		
		if (!border && !tile.Buildable()) return false;
		if (!(type == Terran_Bunker || type == Terran_Missile_Turret))
			if (border && !tile.Buildable()) return false;

		if (!border && tile.GetNeutral()) return false;
		if (!(type == Terran_Missile_Turret))
			if (border && tile.GetNeutral()) return false;

		if (ai()->GetVMap().GetBuildingOn(tile)) return false;
		if (ai()->GetVMap().AddonRoom(tile)) return false;

		if (type == Terran_Missile_Turret)
			if (!border && ai()->GetVMap().SCVTraffic(tile)) return false;

		if (type != Terran_Missile_Turret)
			if (!border)
				if (ai()->GetVMap().FirstTurretsRoom(tile)) return false;

		if (type == Terran_Command_Center)
		{
			if (ai()->GetVMap().CommandCenterImpossible(tile)) return false;
		}
		else
		{
			if (!border)
				if (ai()->GetVMap().CommandCenterWithAddonRoom(tile)) return false;
		}
	}

	verticalAdjacentBuildings = topAdjacentBuildings + bottomAdjacentBuildings;
	if (verticalAdjacentBuildings && (type == Terran_Missile_Turret)) return false;

	return true;
}


MyUnit * findBlockingMine(BWAPI::UnitType type, TilePosition location)
{
	TilePosition dim(type.tileSize());
	if (type.canBuildAddon()) dim.x += 2;

	for (const auto & u : me().Units(Terran_Vulture_Spider_Mine))
		if (u->Completed())
			if (distToRectangle(u->Pos(), location, dim) <= 1)
				return u.get();

	return nullptr;
}



TilePosition Constructing::FindFirstDepotPlacement()
{
	assert_throw(!ai()->GetStrategy()->Active<FirstDepotPlacement>());

	if (auto s = ai()->GetStrategy()->Active<Walling>())
		if (s->GetWall()->Size() >= 2)
			return s->GetWall()->Locations()[1];

	return FindBuildingPlacement(Terran_Supply_Depot, me().Bases()[0], TilePosition(me().Bases()[0]->BWEMPart()->Center()));
}



TilePosition Constructing::FindSecondDepotPlacement()
{
	assert_throw(!ai()->GetStrategy()->Active<SecondDepotPlacement>());

	if (auto s = ai()->GetStrategy()->Active<Walling>())
		if (s->GetWall()->Size() >= 3)
			return s->GetWall()->Locations()[2];

	return FindBuildingPlacement(Terran_Supply_Depot, me().Bases()[0], TilePosition(me().Bases()[0]->BWEMPart()->Center()));
}



TilePosition Constructing::FindFirstBarracksPlacement()
{
	assert_throw(!ai()->GetStrategy()->Active<FirstBarracksPlacement>());

	if (auto s = ai()->GetStrategy()->Active<Walling>())
		if (s->GetWall()->Size() >= 1)
			return s->GetWall()->Locations()[0];

	return FindBuildingPlacement(Terran_Barracks, me().Bases()[0], TilePosition(me().Bases()[0]->BWEMPart()->Center()));
}


TilePosition Constructing::FindFirstFactoryPlacement()
{
	assert_throw(!ai()->GetStrategy()->Active<FirstFactoryPlacement>());

	const UnitType type = Terran_Factory;
	const Area * area = ai()->Me().GetArea();
	const VArea * varea = ai()->Me().GetVArea();

	assert_throw(varea->HotCP());

	TilePosition EnlargedArea_TopLeft     = {numeric_limits<int>::max(), numeric_limits<int>::max()};
	TilePosition EnlargedArea_BottomRight = {numeric_limits<int>::min(), numeric_limits<int>::min()};
	for (const Area * area : varea->EnlargedArea())
	{
		makeBoundingBoxIncludePoint(EnlargedArea_TopLeft, EnlargedArea_BottomRight , area->TopLeft());
		makeBoundingBoxIncludePoint(EnlargedArea_TopLeft, EnlargedArea_BottomRight , area->BottomRight());
	}

	map<double, TilePosition> Candidates;
	
	TilePosition dim(type.tileSize());
	dim.x += 2;

	TilePosition SearchBoundingBox_TopLeft = ai()->GetMap().Crop(EnlargedArea_TopLeft);
	TilePosition SearchBoundingBox_BottomRight = ai()->GetMap().Crop(EnlargedArea_BottomRight);
	SearchBoundingBox_BottomRight -= dim-1;
	--SearchBoundingBox_BottomRight.x;	// no shop on the right edge


//	bw->drawBoxMap(center(SearchBoundingBox_TopLeft), center(SearchBoundingBox_BottomRight), Colors::Orange);

	for (int y = SearchBoundingBox_TopLeft.y ; y <= SearchBoundingBox_BottomRight.y ; ++y)
	for (int x = SearchBoundingBox_TopLeft.x ; x <= SearchBoundingBox_BottomRight.x ; ++x)
	{
		int verticalAdjacentBuildings;
		TilePosition target(x, y);
		TilePosition bottomRight = target + dim-1;

		if (tileInEnlargedArea(target, varea, area))
		if (tileInEnlargedArea(bottomRight, varea, area))
		if (tileInEnlargedArea(TilePosition(target.x, bottomRight.y), varea, area))
		if (tileInEnlargedArea(TilePosition(bottomRight.x, target.y), varea, area))
			if (canBuild(type, target, verticalAdjacentBuildings))
			{
				double score = dist(target + dim/2, TilePosition(varea->HotCP()->Center()))
							 + dist(target + dim/2, TilePosition(ai()->Me().StartingBase()->Center()));
				Candidates[score] = target;
			}

	}

	return Candidates.empty() ? TilePositions::None : Candidates.rbegin()->second;
}


static double nearestTurret(TilePosition fromTile, VBase * base, double tileRadius)
{
	double minTileDist = tileRadius;

	for (const auto & b : me().Buildings(Terran_Missile_Turret))
		if (b->GetStronghold() == base->GetStronghold())
			minTileDist = min(minTileDist, dist(fromTile, b->TopLeft()));

	return minTileDist;
}


static Mineral * farestMineral(TilePosition fromTile, VBase * base)
{
	Mineral * pFarestMineral = nullptr;
	double maxTileDist = numeric_limits<double>::min();

	for (Mineral * m : base->BWEMPart()->Minerals())
	{
		double d = dist(fromTile, m->TopLeft());
		if (d > maxTileDist)
		{
			maxTileDist = d;
			pFarestMineral = m;
		}
	}

	return pFarestMineral;
}


static double nearestMineral(TilePosition fromTile, VBase * base)
{
	double minTileDist = numeric_limits<double>::max();

	for (Mineral * m : base->BWEMPart()->Minerals())
		minTileDist = min(minTileDist, dist(fromTile, m->TopLeft()));

	return minTileDist;
}


TilePosition Constructing::FindBuildingPlacement(BWAPI::UnitType type, VBase * base, TilePosition near, int maxTries)
{
	assert_throw(base);

	if ((type == Terran_Bunker) &&
		(ai()->GetStrategy()->Detected<ZerglingRush>() || him().ZerglingPressure() || him().HydraPressure()) &&
		ai()->Me().GetVArea()->DefenseCP())
		return FindChokePointBunkerPlacement();

	bool liftingCC = false;
	if (type == Terran_Command_Center)
	{
		if (auto s = ai()->GetStrategy()->Active<Expand>())
			if (s->LiftCC())
				liftingCC = true;

		if (!liftingCC)
			return base->BWEMPart()->Location();
	}

	const VArea * varea = base->GetArea();
	const Area * area = varea->BWEMPart();

	bool restrictToBaseEnlargedArea = true;
/*
	if (const MyBuilding * b = me().FindBuildingNeedingBuilder(type))
	{
		//b->CancelConstruction();

		if (m_pExpert) m_pExpert->OnBuildingCreated();
		return b->TopLeft();
	}
*/

//	const bool bunkerForMarineRush =(type == Terran_Bunker) &&
//									ai()->GetStrategy()->Detected<MarineRush>();


	if (type == Terran_Supply_Depot)
	{
		if (const FirstDepotPlacement * pFirstDepotPlacement = ai()->GetStrategy()->Active<FirstDepotPlacement>())
			return pFirstDepotPlacement->Location();

		if (const SecondDepotPlacement * pSecondDepotPlacement = ai()->GetStrategy()->Active<SecondDepotPlacement>())
			return pSecondDepotPlacement->Location();

		if (auto s = ai()->GetStrategy()->Active<Walling>())
			if (!me().LostUnitsOrBuildings(Terran_Supply_Depot))
				if ((int)me().Buildings(type).size() < s->GetWall()->Size() - 1)
					return s->GetWall()->Locations()[1 + me().Buildings(type).size()];
	}

	if (type == Terran_Barracks)
	{

		if (const FirstBarracksPlacement * pFirstBarracksPlacement = ai()->GetStrategy()->Active<FirstBarracksPlacement>())
			return pFirstBarracksPlacement->Location();

		if (me().Buildings(type).empty())
			if (auto s = ai()->GetStrategy()->Active<Walling>())
				if (!me().LostUnitsOrBuildings(Terran_Barracks))
					return s->GetWall()->Locations()[0];
	}

	if (type == Terran_Factory)
		if (const FirstFactoryPlacement * pFirstFactoryPlacement = ai()->GetStrategy()->Active<FirstFactoryPlacement>())
			return pFirstFactoryPlacement->Location();

	if (type == Terran_Refinery)
		return base->BWEMPart()->Geysers().front()->TopLeft();

	vector<MyBuilding *> TurretsInBase;
	for (const auto & b : me().Buildings(Terran_Missile_Turret))
		if (b->GetStronghold() == base->GetStronghold())
			TurretsInBase.push_back(b.get());


	MyBuilding * pAloneTurret = nullptr;
	TilePosition wallTurret = TilePositions::None;
	if (type == Terran_Missile_Turret)
	{

		for (size_t i = 0 ; i < base->FirstTurretsLocations().size() ; ++i)
			if (base->FirstTurretsLocations()[i] != TilePositions::None)
				if (TurretsInBase.size() == i)
					return base->FirstTurretsLocations()[i];

		if (him().IsProtoss())
			if (base->CreationTime() == 0)
				if (auto * s = ai()->GetStrategy()->Has<Walling>())
					if (s->GetWall())
						if (!s->GetWall()->Open())
							if (none_of(TurretsInBase.begin(), TurretsInBase.end(), [s](MyBuilding * turret)
									{ return dist(turret->Pos(), s->GetWall()->Center()) < 6*32; }))
								wallTurret = TilePosition(s->GetWall()->Center());

		if (him().IsZerg())
			if (base->CreationTime() == 0)
				if (TurretsInBase.size() >= 3 && TurretsInBase.size() <= 6)
					pAloneTurret = findAloneTurret(base, 6*32);
	}

	map<double, TilePosition> Candidates;
	int radius = 32;
	
	TilePosition dim(type.tileSize());
	if (type.canBuildAddon()) dim.x += 2;

	for (int tries = 1 ; tries <= maxTries ; ++tries)
	{
		if (tries > 1) restrictToBaseEnlargedArea = false;


		TilePosition SearchBoundingBox_TopLeft = ai()->GetMap().Crop(near - radius);
		TilePosition SearchBoundingBox_BottomRight = ai()->GetMap().Crop(near + radius);
		SearchBoundingBox_BottomRight -= dim-1;
		if (type == Terran_Factory) --SearchBoundingBox_BottomRight.x;		// no shop on the right edge


		//	bw->drawCircleMap(center(near), 10, Colors::Orange);
		//	bw->drawBoxMap(center(SearchBoundingBox_TopLeft), center(SearchBoundingBox_BottomRight), Colors::Orange);
		//	if (m_type == Terran_Command_Center) ai()->SetDelay(10000);

		for (int y = SearchBoundingBox_TopLeft.y ; y <= SearchBoundingBox_BottomRight.y ; ++y)
		for (int x = SearchBoundingBox_TopLeft.x ; x <= SearchBoundingBox_BottomRight.x ; ++x)
		{
			TilePosition target(x, y);
			TilePosition bottomRight = target + dim-1;

			if (restrictToBaseEnlargedArea)
				if (!tileInEnlargedArea(target, varea, area) ||
					!tileInEnlargedArea(bottomRight, varea, area) ||
					!tileInEnlargedArea(TilePosition(target.x, bottomRight.y), varea, area) ||
					!tileInEnlargedArea(TilePosition(bottomRight.x, target.y), varea, area))
					continue;

			int verticalAdjacentBuildings;
			if (canBuild(type, target, verticalAdjacentBuildings))
			{
				Position targetCenter = Position(target) + Position(dim)/2;
				double score = dist(targetCenter, center(near))/32;

				if (type != Terran_Bunker)
					if (!liftingCC)
						if (verticalAdjacentBuildings) score -= 3;

				if (type == Terran_Bunker)
				{
					if (ai()->GetStrategy()->Detected<MarineRush>())
						if (const ChokePoint * pHotCP = me().GetVArea()->HotCP())
						{
							int distToHotCP = groundDist(targetCenter, center(pHotCP->Center()));
							score += distToHotCP/32 *1/2;
						}

					if (const CannonRush * pCannonRush = ai()->GetStrategy()->Detected<CannonRush>())
					{
						int distToHotPositions = pCannonRush->DistanceToHotPositions(targetCenter);
						score += distToHotPositions/32;

						for (HisBuilding * u : him().Buildings(Protoss_Photon_Cannon))
							if (u->RemainingBuildTime() < 400)
								if (u->GetDistanceToTarget(targetCenter, type) <= 7*32)
								{
								///	bw->drawCircleMap(targetCenter, 5, Colors::Red, true);
									score = 1000000;
								}
					}
				}
				else if (type == Terran_Missile_Turret)
				{
					if (wallTurret != TilePositions::None)
					{
						double d = dist(target, wallTurret);
						score += d*3.0;
					}
					else if (pAloneTurret)
					{
						double d = dist(target, pAloneTurret->TopLeft());
						score += max(6.0, d)*2.0;
					}
					else if (TurretsInBase.size() >= 1)
					{
						double distToNearestTurret = nearestTurret(target, base, 10.0);
						score += (10.0 - distToNearestTurret)*1.0;

						if (TurretsInBase.size() == 1)
						{
							Mineral * m = farestMineral(TurretsInBase.front()->TopLeft(), base);
							score += nearestMineral(m->TopLeft(), base)*2.0;
						}
					}
				}
				else
				{
					if (const CannonRush * pCannonRush = ai()->GetStrategy()->Detected<CannonRush>())
						score = -(pCannonRush->DistanceToHotPositions(center(target + dim/2))/32);

					if (liftingCC)
					{
						const Area * pMainArea = me().GetArea();
						const Area * pNaturalArea = findNatural(me().StartingVBase())->BWEMPart()->GetArea();
						if (!pNaturalArea) pNaturalArea = pMainArea;

						const int mainHeight = pMainArea->VeryHighGroundPercentage() > 50 ? 2 : pMainArea->HighGroundPercentage() > 50 ? 1 : 0;
						const int naturalHeight = pNaturalArea->VeryHighGroundPercentage() > 50 ? 2 : pNaturalArea->HighGroundPercentage() > 50 ? 1 : 0;

						if (naturalHeight < mainHeight)
							score += 2 * dist(center(target + dim/2), me().Bases().back()->BWEMPart()->Center())/32;
					}


					score += 1*ai()->GetGridMap().GetMyUnits(target, target + dim - 1).size();
					score += 5*ai()->GetGridMap().GetHisUnits(target, target + dim - 1).size();
				}

				Candidates[score] = target;
			}
		}

		if (!Candidates.empty())
			return Candidates.begin()->second;
		radius += 5;
	}

	return TilePositions::None;
}



// Finds a location for a bunker to place at the DefenseCP
// Implements a simple Dijkstra algorithm to find the location closest to the center of the DefenseCP.
// Code adapted from BWEM::Area::ComputeDistances
//
TilePosition Constructing::FindChokePointBunkerPlacement()
{
	assert_throw(ai()->Me().GetVArea()->DefenseCP());

	Position centerCP = center(ai()->Me().GetVArea()->DefenseCP()->Center());

/*
	if (const MyBuilding * b = me().FindBuildingNeedingBuilder(type))
	{
		//b->CancelConstruction();

		if (m_pExpert) m_pExpert->OnBuildingCreated();
		return b->TopLeft();
	}
*/

	TilePosition dim(UnitType(Terran_Bunker).tileSize());

	map<int, TilePosition> CandidatesByScore;

	TilePosition start(centerCP);

	Tile::UnmarkAll();
	map<TilePosition, int> DistToCenterCC;	// DistToCenterCC[x] != 0 <=> x in ToVisit
	multimap<int, TilePosition> ToVisit;	// a priority queue holding the tiles to visit ordered by their distance to start.
	ToVisit.emplace(1, start);
	DistToCenterCC[start] = 1;

	while (!ToVisit.empty())
	{
		int currentDist = ToVisit.begin()->first;
		TilePosition current = ToVisit.begin()->second;
		const Tile & currentTile = ai()->GetMap().GetTile(current, check_t::no_check);
		assert_throw(DistToCenterCC[current] == currentDist);
		ToVisit.erase(ToVisit.begin());
		currentTile.SetMarked();

		TilePosition placement = current - dim/2;
		Position centerBunker = Position(placement) + Position(dim)/2;

		if (ai()->GetMap().Valid(placement-1) && ai()->GetMap().Valid(placement + dim))
		{
			int verticalAdjacentBuildings;
			if (canBuild(Terran_Bunker, placement, verticalAdjacentBuildings))
			{
				int distToCP = int(0.5 + currentDist * 32 / 10000);
				int altitude = int(0.5 + 0.33*ai()->GetMap().GetTile(current).MinAltitude());
			//	double distToCPend1 = dist(centerBunker, end1CP)/32.0;
			//	double distToCPend2 = dist(centerBunker, end2CP)/32.0;
				int score = (distToCP < 4*32+16) ? 1000000 :
					  distToCP
					- altitude;
				CandidatesByScore[score] = placement;
			}
		}

		for (TilePosition delta : {	TilePosition(-1, -1), TilePosition(0, -1), TilePosition(+1, -1),
									TilePosition(-1,  0),                      TilePosition(+1,  0),
									TilePosition(-1, +1), TilePosition(0, +1), TilePosition(+1, +1)})
		{
			const bool diagonalMove = (delta.x != 0) && (delta.y != 0);
			const int newNextDist = currentDist + (diagonalMove ? 14142 : 10000);

			TilePosition next = current + delta;
			if (ai()->GetMap().Valid(next))
			{
				const Tile & nextTile = ai()->GetMap().GetTile(next, check_t::no_check); 
				if (!nextTile.Marked())
				{
					int & nextTileDistToCC = DistToCenterCC[next];
					if (nextTileDistToCC)	// next already in ToVisit
					{
						if (newNextDist < nextTileDistToCC)		// nextNewDist < nextOldDist
						{	// To update next's distance, we need to remove-insert it from ToVisit:
							auto range = ToVisit.equal_range(nextTileDistToCC);
							auto iNext = find_if(range.first, range.second, [next]
								(const pair<int, TilePosition> & e) { return e.second == next; });
							assert_throw(iNext != range.second);

							ToVisit.erase(iNext);
							nextTileDistToCC = newNextDist;
							ToVisit.emplace(newNextDist, next);
						}
					}
					else if ((nextTile.AreaId() < 0) ||
					         (nextTile.AreaId() > 0) && ext(ai()->GetMap().GetArea(nextTile.AreaId()))->UnderDefenseCP())
					{
						nextTileDistToCC = newNextDist;
						ToVisit.emplace(newNextDist, next);
					}
				}
			}
		}
	}

	if (!CandidatesByScore.empty()) return CandidatesByScore.begin()->second;

	ai()->GetStrategy()->Has<ZerglingRush>()->SetNoLocationForBunkerForDefenseCP();
	return TilePositions::None;
}



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Constructing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Constructing *> Constructing::m_Instances;


Constructing::Constructing(My<Terran_SCV> * pSCV, BWAPI::UnitType type, ConstructingExpert * pExpert)
	: Behavior(pSCV, behavior_t::Constructing), m_type(type), m_pExpert(pExpert)
{
	assert_throw(!pExpert || (!pSCV->GetStronghold() == (pExpert->GetFreeLocation() != TilePositions::None)));
	assert_throw(!pSCV->GetStronghold() || pSCV->GetStronghold()->HasBase());

	m_pBase = pSCV->GetStronghold() ? pSCV->GetStronghold()->HasBase() : nullptr;

	if (type == Terran_Refinery)
	{
		assert_throw(m_pBase && !m_pBase->BWEMPart()->Geysers().empty());
		for (const auto & b : me().Buildings(Terran_Refinery))
		{
			assert_throw(!b->Completed() || (b->TopLeft() != m_pBase->BWEMPart()->Geysers().front()->TopLeft()));
		}
	}

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);
}


Constructing::~Constructing()
{
#if !DEV
	try //3
#endif
	{
		assert_throw(contains(m_Instances, this));
		really_remove(m_Instances, this);

		if (m_pExpert) m_pExpert->OnConstructionAborted();
	}
#if !DEV
	catch(...){} //3
#endif
}


string Constructing::StateName() const
{CI(this);
	switch(State())
	{
	case reachingLocation:	return "reachingLocation";
	case waitingForMoney:	return "waitingForMoney";
	case constructing:		return "constructing";
	default:			return "?";
	}
}


void Constructing::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);
	if (other->TopLeft() == Location()) 
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
}



TilePosition Constructing::FindBuildingPlacement(TilePosition near) const
{CI(this);
	assert_throw(GetBase());


//	if (Type() == Terran_Command_Center)
//		return GetBase()->BWEMPart()->Location();

	if (const MyBuilding * b = me().FindBuildingNeedingBuilder(Type()))
	{
		//b->CancelConstruction();

		if (m_pExpert) m_pExpert->OnBuildingCreated();
		return b->TopLeft();
	}

	static int failures = 0;
	TilePosition res = FindBuildingPlacement(Type(), GetBase(), near);
	if (res == TilePositions::None)
	{
		if (++failures > 100)
		{
			res = FindBuildingPlacement(Type(), GetBase(), near, 10);
			if (res != TilePositions::None) m_timeToReachLocation = 1000;
			if (failures > 120) failures = 0;
		}
	}
	else
		failures = 0;

	return res;
}


const Tile & Constructing::LocationTile() const
{CI(this);
	assert_throw(Location() != TilePositions::None);

	return ai()->GetMap().GetTile(Location());
}


BWAPIUnit * Constructing::FindBuilding() const
{CI(this);
	return ai()->GetVMap().GetBuildingOn(LocationTile());
}


bool Constructing::CanRepair(const MyBWAPIUnit * pTarget, int ) const
{
	if (pTarget->IsMyBuilding() && pTarget->IsMyBuilding()->GetWall())
		if ((Location() == TilePositions::None) || !FindBuilding())
			return true;

	return false;
}


void Constructing::OnFrame_v()
{CI(this);
#if DEV
	if (Location() != TilePositions::None) bw->drawBoxMap(Position(Location()), Position(Location() + Type().tileSize()), GetColor());//1
#endif

	if (!Agent()->CanAcceptCommand()) return;

	switch (State())
	{
	case reachingLocation:		OnFrame_reachingLocation(); break;
	case waitingForMoney:		OnFrame_waitingForMoney(); break;
	case constructing:			OnFrame_constructing(); break;
	default: assert_throw(false);
	}
}



void Constructing::OnFrame_reachingLocation()
{CI(this);
	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
		m_location = Agent()->GetStronghold()
			? FindBuildingPlacement(TilePosition(GetBase()->BWEMPart()->Center()))
			: m_pExpert->GetFreeLocation();

		if (m_location == TilePositions::None)
		{
			if (m_pExpert) m_pExpert->OnNoMoreLocation();
		///	bw << "aaa" << endl;
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		}

		SetSubBehavior<Walking>(Agent(), CenterLocation(), __FILE__ + to_string(__LINE__));
	}


	if (Agent()->Life() < Agent()->PrevLife(30))
		if (!ai()->GetStrategy()->Detected<CannonRush>())
			return Agent()->ChangeBehavior<Fleeing>(Agent());


	if (Agent()->GetStronghold())
		if (m_pExpert)
			if (m_pExpert->CanChangeBuilder())
			{
				My<Terran_SCV> * pCloserWorker = findFreeWorker(GetBase(), [this](const My<Terran_SCV> * pGatherer)
							{ return dist(pGatherer->Pos(), CenterLocation()) < dist(Agent()->Pos(), CenterLocation() - 5); },
																bool("canTakeBlocking"));

				if (pCloserWorker)
				{
					m_pExpert->OnBuilderChanged(pCloserWorker);
					pCloserWorker->ChangeBehavior<Constructing>(pCloserWorker, Type(), m_pExpert);
					m_pExpert = nullptr;
				///	bw << "bbb" << endl;
					return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
				}
			}

	bool buildingExists = false;
	if (auto * b = FindBuilding())
		if (b->Is(Type()))
			if (!b->Completed())
				buildingExists = true;

	if (Agent()->Pos().getApproxDistance(GetSubBehavior()->IsWalking()->Target()) < (buildingExists || (m_type == Terran_Refinery) ? 4 : 2) * 32)
	//	if (Agent()->Life() >= 30)
		{
			ResetSubBehavior();
			return ChangeState(waitingForMoney);
		}

	if (ai()->Frame() - m_inStateSince > (buildingExists ? 1000 : 200))
	{
	///	bw << "ccc" << endl;
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}
}



void Constructing::OnFrame_waitingForMoney()
{CI(this);
	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
	}

	bool buildingExists = false;
	if (auto * b = FindBuilding())
		if (b->Is(Type()))
			if (!b->Completed())
				buildingExists = true;


	if (ai()->Frame() - m_inStateSince > (buildingExists ? 1000 : 250))
	{
	///	bw << "ddd" << endl;
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}

	if (Agent()->Life() < Agent()->PrevLife(30))
		return Agent()->ChangeBehavior<Fleeing>(Agent());

	if (Agent()->Unit()->isConstructing())
		return ChangeState(constructing);


	if (buildingExists)
	{
		Agent()->RightClick(FindBuilding(), check_t::no_check);
	}
	else
	{
		if (me().CanPay(Type()))
			if (!Agent()->Build(Type(), Location(), check_t::no_check))
				if (MyUnit * pBlockingMine = findBlockingMine(Type(), Location()))
					Agent()->Attack(pBlockingMine);
	}


}



void Constructing::OnFrame_constructing()
{CI(this);
	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
	}

	if (Agent()->Life() < Agent()->PrevLife(30))
//	if (Agent()->Life() < 20)
	{
		bool flee = true;

		if (Agent()->Unit()->isConstructing())
			if (BWAPIUnit * b = FindBuilding())
				if (b->Is(Type()))
				{
					if (Type() == Terran_Missile_Turret)
						if (Agent()->Life() > b->RemainingBuildTime()/3)
							flee = false;

					if (b->IsMyBuilding()->GetWall())
						flee = false;
				}

		if (flee)
			if (findThreatsButWorkers(Agent(), 2*32).empty())
				flee = false;

		if (flee)
			if (ai()->GetStrategy()->Detected<CannonRush>())
				flee = false;

		if (flee)
			return Agent()->ChangeBehavior<Fleeing>(Agent());
	}


	if (Agent()->Unit()->isConstructing())
	{
		if (BWAPIUnit * b = FindBuilding())
			if (b->Is(Type()))
				if (!b->Completed())
					if (b->IsMyBuilding()->CanAcceptCommand())
						if (ai()->GetVMap().AddonRoom(LocationTile()))
						{
						//	bw << "constructing on Addon room !" << endl;
							b->IsMyBuilding()->CancelConstruction();
							return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
						}
	}
	else
	{
		if (ai()->Frame() - m_inStateSince > 30)
		{
			if (!FindBuilding())
			{
			//	bw << Agent()->NameWithId() << " stop constructing..." << endl;
			//	ai()->SetDelay(3333);
			}

		///	bw << "eee" << endl;
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		}
	}
}






} // namespace iron



