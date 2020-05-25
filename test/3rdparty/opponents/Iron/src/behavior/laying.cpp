//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "laying.h"
#include "fleeing.h"
#include "defaultBehavior.h"
#include "../units/factory.h"
#include "../territory/vgridMap.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Laying
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Laying *> Laying::m_Instances;


static bool unitsHere(Position pos, int minesMinSpacing, const vector<MyUnit *> & MyUnitsAround, const vector<HisUnit *> & HisUnitsAround)
{
	for (const MyUnit * u : MyUnitsAround)
	{
		int d = roundedDist(u->Pos(), pos);
		if (d < (u->Is(Terran_Vulture_Spider_Mine) ? minesMinSpacing : max(u->Type().width(), u->Type().height())))
			return true;
	}

	for (const HisUnit * u : HisUnitsAround)
	{
		int d = roundedDist(u->Pos(), pos);
		if (d < max(u->Type().width(), u->Type().height()))
			return true;
	}

	return false;
}


Position Laying::FindMinePlacement(const MyUnit * pSCV, Position near, int radius, int minSpacing)
{
	vector<Position> MinesAround;

	const TilePosition topLeft = ai()->GetMap().Crop(TilePosition(near - (radius + minSpacing)));
	const TilePosition bottomRight = ai()->GetMap().Crop(TilePosition(near + (radius + minSpacing)));
	vector<MyUnit *> MyUnitsAround = ai()->GetGridMap().GetMyUnits(topLeft, bottomRight);
	vector<HisUnit *> HisUnitsAround = ai()->GetGridMap().GetHisUnits(topLeft, bottomRight);

	really_remove(MyUnitsAround, const_cast<MyUnit *>(pSCV));

	for (const auto & u : me().Units(Terran_Vulture_Spider_Mine))
		if (queenWiseDist(u->Pos(), near) < radius + minSpacing)
			MinesAround.push_back(u->Pos());

	for (int r = 0 ; r < radius ; ++r)
		for (int tries = 3*r ; tries >= 0 ; --tries)
		{
			Position cand(near.x + rand()%(2*r+1) - r, near.y + rand()%(2*r+1) - r);

			if (!ai()->GetMap().Valid(cand)) continue;
			if (!ai()->GetMap().GetMiniTile(WalkPosition(cand), check_t::no_check).Walkable()) continue;
			const Tile & tile = ai()->GetMap().GetTile(TilePosition(cand), check_t::no_check);
			if (tile.GetNeutral() || ai()->GetVMap().GetBuildingOn(tile)) continue;
			if (unitsHere(cand, minSpacing, MyUnitsAround, HisUnitsAround)) continue;

			return cand;
		}

	return Positions::None;
}




Laying::Laying(MyUnit * pAgent)
	: Behavior(pAgent, behavior_t::Laying), m_lastMineCreated(ai()->Frame())
{
	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

///	ai()->SetDelay(100);
}


Laying::~Laying()
{
#if !DEV
	try //3
#endif
	{
		assert_throw(contains(m_Instances, this));
		really_remove(m_Instances, this);
	}
#if !DEV
	catch(...){} //3
#endif
}


void Laying::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string Laying::StateName() const
{CI(this);
	switch(State())
	{
	case laying:			return "(" + to_string(TimeElapsedSinceLastMineCreated()) + ")";
	default:				return "?";
	}
}


frame_t Laying::TimeElapsedSinceLastMineCreated() const
{
	return ai()->Frame() - m_lastMineCreated;
}


void Laying::OnFrame_v()
{CI(this);
#if DEV
	if (m_target != Positions::None)
	{
		bw->drawCircleMap(m_target, 5, GetColor(), bool("isSolid"));
		drawLineMap(Agent()->Pos(), m_target, GetColor());
	}
#endif

	if (!Agent()->CanAcceptCommand()) return;

	My<Terran_Vulture> * pVulture = Agent()->IsMy<Terran_Vulture>();
	assert_throw(pVulture);

	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
		m_oldRemainingMines = pVulture->RemainingMines();
		m_lastMineCreated = ai()->Frame();
		m_tries = 0;

//		m_target = Agent()->Pos();
//		m_lastPlacementOrder = ai()->Frame();
//		bw << Agent()->NameWithId() << " lay initial mine" << endl;
//		return pVulture->PlaceMine(m_target);
	}

	{
		auto Threats5 = findThreats(Agent(), 5*32);
		bool enemyTargetedByMines = false;
		for (const auto * pFaceOff : Threats5)
			if (auto * pHisUnit = pFaceOff->His()->IsHisUnit())
				if (!pHisUnit->MinesTargetingThis().empty() && (pFaceOff->GroundDistanceToHitHim() < 6*32))
					enemyTargetedByMines = true;
		if (enemyTargetedByMines) return Agent()->ChangeBehavior<Fleeing>(Agent());
	}


	if (pVulture->RemainingMines() < m_oldRemainingMines)
	{
		m_oldRemainingMines = pVulture->RemainingMines();
		++m_minesLayed;
	///	bw << "Mine created at " << TimeElapsedSinceLastMineCreated() << endl;

		if (pVulture->RemainingMines() == 0)
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

		m_lastMineCreated = ai()->Frame();
		m_tries = 0;
	}


	Vect v = Agent()->Acceleration();
//	bw << "          acc = " << v.Norm() << " " << (Agent()->Unit()->isMoving() ? "(moving)" : "") << endl;

	double k = 13 + 3.5*bw->getRemainingLatencyFrames();
	if (m_tries == 0)
	{
		if (Agent()->Unit()->isMoving())
		{
			if ((m_minesLayed >= 1) && (TimeElapsedSinceLastMineCreated() < 2)) return;
			if (v.Norm() < 3) return;

			v.Normalize();
			m_target = FindMinePlacement(Agent(), ai()->GetMap().Crop(Agent()->Pos() + toPosition(v * k)), 50, 16);
		///	bw << Agent()->NameWithId() << " lay next mine forward" << endl;
		}
		else
		{
			if ((m_minesLayed >= 1) && (TimeElapsedSinceLastMineCreated() < 2)) return;

			m_target = FindMinePlacement(Agent(), ai()->GetMap().Crop(Agent()->Pos()), 50, 16);
		///	bw << Agent()->NameWithId() << " lay next mine nearby" << endl;
		}

		++m_tries;
		m_lastPlacementOrder = ai()->Frame();
		return pVulture->PlaceMine(m_target);
	}
	else
	{
		if (Agent()->Unit()->isMoving())
		{
			int d0 = roundedDist(Agent()->Pos(), m_target);
			int d1 = roundedDist(Agent()->PrevPos(1), m_target);
			int d2 = roundedDist(Agent()->PrevPos(2), m_target);
			if (!((d0 > k) && (d0 > d1) && (d1 > d2))) return;

			v.Normalize();
			m_target = FindMinePlacement(Agent(), ai()->GetMap().Crop(Agent()->Pos() + toPosition(v * k)), 50, 16);
		///	bw << Agent()->NameWithId() << " lay again next mine forward" << endl;
		}
		else
		{
			if (ai()->Frame() - m_lastPlacementOrder < 13 + bw->getRemainingLatencyFrames()) return;

			m_target = FindMinePlacement(Agent(), ai()->GetMap().Crop(Agent()->Pos()), 50, 16);
		///	bw << Agent()->NameWithId() << " lay again next mine nearby" << endl;
		}

		++m_tries;
		if (m_tries == 4) return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

		m_lastPlacementOrder = ai()->Frame();
		return pVulture->PlaceMine(m_target);
	}

	if (ai()->Frame() - m_inStateSince > 100)
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
}



} // namespace iron



