//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "vbase.h"
#include "stronghold.h"
#include "../territory/vgridmap.h"
#include "../units/cc.h"
#include "../units/refinery.h"
#include "../behavior/constructing.h"
#include "../behavior/behavior.h"
#include "../behavior/mining.h"
#include "../behavior/refining.h"
#include "../behavior/supplementing.h"
#include "../behavior/exploring.h"
#include "../behavior/blocking.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }

namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class VBase
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


VBase * VBase::Get(const BWEM::Base * pBWEMPart)
{
	return static_cast<VBase *>(pBWEMPart->Ext());
}


VBase::VBase(const BWEM::Base * pBWEMPart)
	: m_pBWEMPart(pBWEMPart)
{
	assert_throw(pBWEMPart);
	assert_throw(!ext(m_pBWEMPart));
	m_pBWEMPart->SetExt(this);

	ComputeGuardingCorners();
}


void VBase::Initialize()
{
	ComputeFirstTurretLocation();
}


VBase::~VBase()
{
	m_pBWEMPart->SetExt(nullptr);

}


const VArea * VBase::GetArea() const
{
	return ext(BWEMPart()->GetArea());
}


bool VBase::ShouldRebuild() const
{
//	const GridMapCell & Cell = ai()->GetGridMap().GetCell(BWEMPart()->Center());
	// TODO

	return ai()->Frame() - LostTime() > 500;
}


void VBase::SetCC(My<Terran_Command_Center> * pCC)
{
	assert_throw(!pCC != !m_pCC);
	
	if (pCC)
	{
		m_lost = false;
	}
	else
	{
		assert_throw(!m_lost);
		if (Active())
		{
			m_lost = true;
			m_LostTime = ai()->Frame();
		}
	}

	m_pCC = pCC;
}


bool VBase::Active() const
{
	if (GetCC())
		if (GetCC()->Completed())
			if (!GetCC()->Flying())
				if (GetCC()->TopLeft() == BWEMPart()->Location())
					return true;

	return false;
}


void VBase::SetCreationTime()
{
	m_creationTime = ai()->Frame();
}


void VBase::OnUnitIn(MyUnit * u)
{
	assert_throw(u);
	assert_throw(GetStronghold());
	assert_throw(u->GetStronghold() == GetStronghold());

	if (Mining * pMiner = u->GetBehavior()->IsMining()) PUSH_BACK_UNCONTAINED_ELEMENT(m_Miners, pMiner);
	else if (Refining * pRefiner = u->GetBehavior()->IsRefining()) PUSH_BACK_UNCONTAINED_ELEMENT(m_Refiners, pRefiner);
	else if (Supplementing * pSupplementer = u->GetBehavior()->IsSupplementing()) PUSH_BACK_UNCONTAINED_ELEMENT(m_Supplementers, pSupplementer);
}


void VBase::OnBuildingIn(MyBuilding * b)
{
	assert_throw(b);
	assert_throw(GetStronghold());
	assert_throw(b->GetStronghold() == GetStronghold());
	
	if (auto * pCC = b->IsMy<Terran_Command_Center>())
	{
		if (b->TopLeft() == BWEMPart()->Location())
			SetCC(pCC);
	}
	else if (auto * pRefinery = b->IsMy<Terran_Refinery>())
	{
		bool foundGeyser = false;
		for (Geyser * g : BWEMPart()->Geysers())
			if (pRefinery->TopLeft() == g->TopLeft())
			{
				assert_throw(!g->Ext());
				g->SetExt(pRefinery);
				pRefinery->SetGeyser(g);
				foundGeyser = true;
				break;
			}

		assert_throw(foundGeyser);
	}
}


void VBase::OnUnitOut(MyUnit * u)
{
	assert_throw(u);
	assert_throw(GetStronghold());
	assert_throw(u->GetStronghold() == GetStronghold());

	if (u->HasBehavior())
		if (Mining * pMiner = u->GetBehavior()->IsMining())
		{
			auto i = find(m_Miners.begin(), m_Miners.end(), pMiner);
			assert_throw(i != m_Miners.end());
			fast_erase(m_Miners, distance(m_Miners.begin(), i));
		}
		else if (Refining * pRefiner = u->GetBehavior()->IsRefining())
		{
			auto i = find(m_Refiners.begin(), m_Refiners.end(), pRefiner);
			assert_throw(i != m_Refiners.end());
			fast_erase(m_Refiners, distance(m_Refiners.begin(), i));
		}
		else if (Supplementing * pSupplementer = u->GetBehavior()->IsSupplementing())
		{
			auto i = find(m_Supplementers.begin(), m_Supplementers.end(), pSupplementer);
			assert_throw(i != m_Supplementers.end());
			fast_erase(m_Supplementers, distance(m_Supplementers.begin(), i));
		}
}


void VBase::OnBuildingOut(MyBuilding * b)
{
	assert_throw(b);
	assert_throw(GetStronghold());
	assert_throw(b->GetStronghold() == GetStronghold());

	if (auto * pCC = b->IsMy<Terran_Command_Center>())
	{
		//if (b->TopLeft() == BWEMPart()->Location())
		if (GetCC() == b)
			SetCC(nullptr);
	}
	else if (auto * pRefinery = b->IsMy<Terran_Refinery>())
	{
		assert_throw(pRefinery->GetGeyser());
		assert_throw(pRefinery->GetGeyser()->Ext());
		assert_throw(pRefinery->GetGeyser()->Ext() == pRefinery);
		pRefinery->GetGeyser()->SetExt(nullptr);
		pRefinery->SetGeyser(nullptr);
	}
}


int VBase::MineralsAmount() const
{
	int amount = 0;

	for (const Mineral * m : BWEMPart()->Minerals())
		amount += m->Amount();

	return amount;
}


int VBase::GasAmount() const
{
	if (BWEMPart()->Geysers().empty()) return 0;
	return BWEMPart()->Geysers().front()->Amount();
}


void VBase::AddMiner(Mining * pMiner)
{
	assert_throw(pMiner->Agent()->GetStronghold() == GetStronghold());

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Miners, pMiner);
}


void VBase::RemoveMiner(Mining * pMiner)
{
	assert_throw(pMiner->Agent()->GetStronghold() == GetStronghold());

	auto it = find(m_Miners.begin(), m_Miners.end(), pMiner);
	assert_throw(it != m_Miners.end());
	fast_erase(m_Miners, distance(m_Miners.begin(), it));
}


int VBase::MaxMiners() const
{
	if (Active())
		return 2 * BWEMPart()->Minerals().size();

	return 0;
}


int VBase::MaxRefiners() const
{
	int geysers = BWEMPart()->Geysers().size();
	if (geysers == 0) return 0;

	if (Active())
		if (My<Terran_Refinery> * pRefinery = static_cast<My<Terran_Refinery> *>(BWEMPart()->Geysers().front()->Ext()))
			if (pRefinery->Is(Terran_Refinery))
				if (pRefinery->Completed())
					return 3;

	return 0;
}


void VBase::AddRefiner(Refining * pRefiner)
{
	assert_throw(pRefiner->Agent()->GetStronghold() == GetStronghold());

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Refiners, pRefiner);
}


void VBase::RemoveRefiner(Refining * pRefiner)
{
	assert_throw(pRefiner->Agent()->GetStronghold() == GetStronghold());

	auto it = find(m_Refiners.begin(), m_Refiners.end(), pRefiner);
	assert_throw(it != m_Refiners.end());
	fast_erase(m_Refiners, distance(m_Refiners.begin(), it));
}


void VBase::AddSupplementer(Supplementing * pSupplementer)
{
	assert_throw(pSupplementer->Agent()->GetStronghold() == GetStronghold());

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Supplementers, pSupplementer);
}


void VBase::RemoveSupplementer(Supplementing * pSupplementer)
{
	assert_throw(pSupplementer->Agent()->GetStronghold() == GetStronghold());

	auto it = find(m_Supplementers.begin(), m_Supplementers.end(), pSupplementer);
	assert_throw(it != m_Supplementers.end());
	fast_erase(m_Supplementers, distance(m_Supplementers.begin(), it));
}


bool VBase::CheckSupplementerAssignment()
{
	if (!Supplementers().empty())
	{
		My<Terran_SCV> * pSupplementer = Supplementers().front()->Agent();
		if (LackingMiners() > 0)
		{
			pSupplementer->ChangeBehavior<Mining>(pSupplementer);
			return true;
		}
		if (LackingRefiners() > 0)
		{
			pSupplementer->ChangeBehavior<Refining>(pSupplementer);
			return true;
		}
	}

	return false;
}


int VBase::OtherSCVs() const
{
	return (int)GetStronghold()->SCVs().size() - (int)Miners().size() - (int)Refiners().size() - (int)Supplementers().size();
}


Mineral * VBase::FindVisibleMineral() const
{
	for (Mineral * m : BWEMPart()->Minerals())
		if (m->Unit()->isVisible())
			return m;

	return nullptr;
}


void VBase::UpdateLastTimePatrolled() const
{
	m_lastTimePatrolled = ai()->Frame();
}


bool VBase::BlockedByOneOfMyMines() const
{
	return findBlockingMine(Terran_Command_Center, BWEMPart()->Location()) != nullptr;
}


void VBase::ComputeGuardingCorners()
{
	TilePosition avgMineralPos(0, 0);

	if (!BWEMPart()->Minerals().empty())
	{
		for (Mineral * m : BWEMPart()->Minerals())
			avgMineralPos += m->TopLeft();

		avgMineralPos /= BWEMPart()->Minerals().size();
	}

	multimap<int, TilePosition> Candidates;

	TilePosition location = BWEMPart()->Location();
	TilePosition dim = UnitType(Terran_Command_Center).tileSize();
	for (TilePosition t : {	location-1,
							location+dim,
							TilePosition((location-1).x, (location+dim).y),
							TilePosition((location+dim).x, (location-1).y)})
		Candidates.emplace(roundedDist(t, avgMineralPos), t);

	m_GuardingCorners.first = Candidates.rbegin()->second;
	m_GuardingCorners.second = (++Candidates.rbegin())->second;
}


Position nearestEmpty(Position pos)
{
	return center(ai()->GetMap().BreadthFirstSearch(TilePosition(pos),
							[](const Tile & tile, TilePosition) { return tile.Walkable() && !tile.GetNeutral() && !ai()->GetVMap().NearBuildingOrNeutral(tile) && !ai()->GetVMap().GetBuildingOn(tile); },	// findCond
							[](const Tile &,      TilePosition) { return true; }));					// visitCond
}



My<Terran_SCV> * findFreeWorker(const VBase * base, function<bool(const My<Terran_SCV> *)> pred, bool canTakeBlocking, bool urgent)
{
	assert_throw(base);

	My<Terran_SCV> * pFreeWorker = nullptr;
		
	if (!pFreeWorker)
		for (Supplementing * s : base->Supplementers())
			if (pred(s->Agent()))
			{
				pFreeWorker = s->Agent();
				break;
			}
		
	if (!pFreeWorker)
		if (base->Miners().size() > 3)
			for (Mining * m : base->Miners())
				if (m->MovingTowardsMineral() || urgent)
					if (pred(m->Agent()))
					{
						pFreeWorker = m->Agent();
						break;
					}

	if (!pFreeWorker)
		for (Refining * r : base->Refiners())
			if (r->MovingTowardsRefinery())
				if (pred(r->Agent()))
				{
					pFreeWorker = r->Agent();
					break;
				}

	if (!pFreeWorker)
		for (Exploring * e : Exploring::Instances())
			if (My<Terran_SCV> * pSCV = e->Agent()->IsMy<Terran_SCV>())
				if (pSCV->GetStronghold() == base->GetStronghold())
					if (pred(pSCV))
					{
						pFreeWorker = pSCV;
						break;
					}

	if (!pFreeWorker)
		if (canTakeBlocking)
			for (Blocking * b : Blocking::Instances())
				if (My<Terran_SCV> * pSCV = b->Agent()->IsMy<Terran_SCV>())
					if (pSCV->GetStronghold() == base->GetStronghold())
						if (pred(pSCV))
						{
							pFreeWorker = pSCV;
							break;
						}

	return pFreeWorker;
}


static bool canBuildTurret(TilePosition location)
{
	TilePosition dim(UnitType(Terran_Missile_Turret).tileSize());

	CHECK_POS(location);
	CHECK_POS(location + dim - 1);

	for (int dy = -1 ; dy <= dim.y ; ++dy)
	for (int dx = -1 ; dx <= dim.x ; ++dx)
	{
		TilePosition t = location + TilePosition(dx, dy);

		bool border = (dx == -1) || (dy == -1) || (dx == dim.x) || (dy == dim.y);
		if (border)
			if (!ai()->GetMap().Valid(t)) continue;

		const auto & tile = ai()->GetMap().GetTile(t);
		if (!border && !tile.Buildable()) return false;
		if (!border && tile.GetNeutral()) return false;
		if (ai()->GetVMap().CommandCenterWithAddonRoom(tile)) return false;
		if (!border && ai()->GetVMap().SCVTraffic(tile)) return false;
		if (!border && ai()->GetVMap().FirstTurretsRoom(tile)) return false;
	}

	return true;
}


void VBase::ComputeFirstTurretLocation()
{
	fill(m_firstTurretsLocations.begin(), m_firstTurretsLocations.end(), TilePositions::None);

	TilePosition ccDim = UnitType(Terran_Command_Center).tileSize();
	TilePosition turretDim = UnitType(Terran_Missile_Turret).tileSize();

	TilePosition ccLoc = BWEMPart()->Location();
	Position ccCenter = BWEMPart()->Center();

	TilePosition refineryLoc = BWEMPart()->Geysers().empty() ? TilePositions::None : BWEMPart()->Geysers().front()->TopLeft();

	vector<Ressource *> AssignedRessources(BWEMPart()->Minerals().begin(), BWEMPart()->Minerals().end());
	AssignedRessources.insert(AssignedRessources.end(), BWEMPart()->Geysers().begin(), BWEMPart()->Geysers().end());

	vector<Position> RessourceTargets;

	for (const Mineral * m : BWEMPart()->Minerals())
		RessourceTargets.push_back(m->Pos());

	for (const Geyser * g : BWEMPart()->Geysers())
	{
		RessourceTargets.push_back(g->Pos());

		Position geyserExitTarget = g->Pos();
		if (g->Pos().y < ccCenter.y - 4*32) geyserExitTarget.x += 50;
		if (g->Pos().y > ccCenter.y + 4*32) geyserExitTarget.x -= 50;
		if (g->Pos().x < ccCenter.x - 4*32) geyserExitTarget.y -= 25;
		if (g->Pos().x > ccCenter.x + 4*32) geyserExitTarget.y += 25;
		RessourceTargets.push_back(geyserExitTarget);
	}

	vector<TilePosition> TrafficTiles;
	for (Position target : RessourceTargets)
	{
		const int N = roundedDist(ccCenter, target) / 8;
		for (int i = N ; i >= 0 ; --i)
		{
			Position p = (ccCenter*i + target*(N - i)) / N;
			TilePosition t(p);

			const Tile & tile = ai()->GetMap().GetTile(t);
			if (const Neutral * n = tile.GetNeutral())
			{
			///	if (n->IsGeyser()) MapPrinter::Get().Rectangle(WalkPosition(t), WalkPosition(t) + 3, MapPrinter::Color(155, 155, 155), MapPrinter::fill);
				break;
			}

			ai()->GetVMap().SetSCVTraffic(tile);
			TrafficTiles.push_back(t);
		///	MapPrinter::Get().Rectangle(WalkPosition(t), WalkPosition(t) + 3, MapPrinter::Color(255, 255, 255), MapPrinter::fill);
		}
	}
/*
	for (Position target : RessourceTargets)
	{
		const int N = roundedDist(ccCenter, target) / 8;
		for (int i = N ; i >= 0 ; --i)
		{
			Position p = (ccCenter*i + target*(N - i)) / N;
			TilePosition t(p);

			if (ai()->GetMap().GetTile(t).GetNeutral()) continue;
			MapPrinter::Get().Point(WalkPosition(p), MapPrinter::Color(0, 0, 255));
		}
	}
*/
	TilePosition SearchBoundingBox_TopLeft = ai()->GetMap().Crop(ccLoc - 6 - 1) + 1;
	TilePosition SearchBoundingBox_BottomRight = ai()->GetMap().Crop(ccLoc + 6 + ccDim - turretDim + 1) - 1;

	for (int i = 0 ; i < (int)m_firstTurretsLocations.size() ; ++i)
	{
		int bestScore = numeric_limits<int>::max();

		for (int y = SearchBoundingBox_TopLeft.y ; y <= SearchBoundingBox_BottomRight.y ; ++y)
		for (int x = SearchBoundingBox_TopLeft.x ; x <= SearchBoundingBox_BottomRight.x ; ++x)
		{
			TilePosition loc(x, y);
			TilePosition bottomRight = loc + turretDim-1;

			if (!tileInEnlargedArea(loc, GetArea(), GetArea()->BWEMPart()) ||
				!tileInEnlargedArea(bottomRight, GetArea(), GetArea()->BWEMPart()) ||
				!tileInEnlargedArea(TilePosition(loc.x, bottomRight.y), GetArea(), GetArea()->BWEMPart()) ||
				!tileInEnlargedArea(TilePosition(bottomRight.x, loc.y), GetArea(), GetArea()->BWEMPart()))
				continue;

			if (canBuildTurret(loc))
			{
			///	MapPrinter::Get().Point(WalkPosition(loc+1), MapPrinter::Color(0, 0, 0));

				int score = 0;
				for (TilePosition t : TrafficTiles)
					score += roundedDist(center(loc), center(t));

				for (int j = 0 ; j < i ; ++j)
					score -= roundedDist(center(loc), center(m_firstTurretsLocations[j])) * TrafficTiles.size() / 2;
			
				if (score < bestScore)
				{
					bestScore = score;
					m_firstTurretsLocations[i] = loc;
				}
			}
		}

		if (m_firstTurretsLocations[i] != TilePositions::None)
		{
		///	MapPrinter::Get().Rectangle(WalkPosition(FirstTurretLocation()), WalkPosition(FirstTurretLocation()) + 7, MapPrinter::Color(0, 0, 0));

			for (int dy = 0 ; dy < turretDim.y ; ++dy)
			for (int dx = 0 ; dx < turretDim.x ; ++dx)
			{
				TilePosition t = m_firstTurretsLocations[i] + TilePosition(dx, dy);
				ai()->GetVMap().SetFirstTurretsRoom(ai()->GetMap().GetTile(t));
			}
		}
	}
}


void VBase::AssignWall()
{
	Wall * pClosestWall = nullptr;
	int minDist = 32*64;

	for (const Area & area : ai()->GetMap().Areas())
		for (const ChokePoint * cp : area.ChokePoints())

		if (ext(cp)->GetWall().Possible())
		{
			int d = groundDist(center(cp->Center()), BWEMPart()->Center());
			if (d < minDist)
			{
				minDist = d;
				pClosestWall = &ext(cp)->GetWall();
			}
		}

	m_pWall = pClosestWall;
}


} // namespace iron



