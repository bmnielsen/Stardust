//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "destroying.h"
#include "defaultBehavior.h"
#include "laying.h"
#include "fleeing.h"
#include "../units/factory.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Destroying
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Destroying *> Destroying::m_Instances;


Destroying::Destroying(My<Terran_Vulture> * pAgent, const vector<HisUnit *> & Targets)
	: Behavior(pAgent, behavior_t::Destroying), m_Targets(Targets)
{
	assert_throw(!Targets.empty());
	assert_throw(none_of(Targets.begin(), Targets.end(), [](HisUnit * u){ return u->InFog(); }));

	for (HisUnit * u : m_Targets)
		u->AddDestroyer(this);

	m_DestroyingSince = ai()->Frame();

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

//	{ bw << pAgent->NameWithId() << " destroying " << Targets.front()->NameWithId() << endl; ai()->SetDelay(500); }
}


Destroying::~Destroying()
{
#if !DEV
	try //3
#endif
	{
		for (HisUnit * u : m_Targets)
			u->RemoveDestroyer(this);

		assert_throw(contains(m_Instances, this));
		really_remove(m_Instances, this);
	}
#if !DEV
	catch(...){} //3
#endif
}


void Destroying::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string Destroying::StateName() const
{CI(this);
	switch(State())
	{
	case destroying:
		if (m_lastMinePlacement)
			return "laying mine (" + to_string(MinePlacementTime() - (ai()->Frame() - m_lastMinePlacement)) + ")";

		return "approaching";

	default:			return "?";
	}
}


frame_t Destroying::MinePlacementTime() const
{CI(this);
	return 18;
}


void Destroying::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);
	if (HisUnit * pHisUnit = other->IsHisUnit())
		if (contains(m_Targets, pHisUnit))
		{
			really_remove(m_Targets, pHisUnit);
			pHisUnit->RemoveDestroyer(this);

			if (m_Targets.empty())
			{
				return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
			}
		}
}


Vect Destroying::CalcultateDivergingVector() const
{CI(this);
	Destroying * pCloser = nullptr;
	const int maxDist = 32*10;
	int minDist = maxDist;

	for (Destroying * pDestroying : Instances())
		if (pDestroying != this)
		{
			int d = Agent()->Pos().getApproxDistance(pDestroying->Agent()->Pos());
			if (d < minDist) 
			{
				minDist = d;
				pCloser = pDestroying;
			}
		}

	Vect dir;
	if (pCloser)
	{
		dir = toVect(Agent()->Pos() - pCloser->Agent()->Pos());
		dir.Normalize();
		dir *= ((maxDist - minDist) / (double)maxDist);
	}

	return dir;
}


void Destroying::OnFrame_v()
{CI(this);
	assert_throw(!m_Targets.empty());

#if DEV
	Position topLeftTargets     = {numeric_limits<int>::max(), numeric_limits<int>::max()};
	Position bottomRightTargets = {numeric_limits<int>::min(), numeric_limits<int>::min()};
	for (HisUnit * u : m_Targets)
	{
		makeBoundingBoxIncludePoint(topLeftTargets, bottomRightTargets, Position(u->GetLeft(), u->GetTop()));
		makeBoundingBoxIncludePoint(topLeftTargets, bottomRightTargets, Position(u->GetRight(), u->GetBottom()));
	}
	bw->drawBoxMap(topLeftTargets, bottomRightTargets, GetColor());
#endif

	if (!Agent()->CanAcceptCommand()) return;

	switch (State())
	{
	case destroying:		OnFrame_destroying(); break;
	default: assert_throw(false);
	}
}


void Destroying::OnFrame_destroying()
{CI(this);
	if (JustArrivedInState())
	{
		m_DestroyingSince = ai()->Frame();
	}
/*
	if (m_lastMinePlacement)
	{
		if (ai()->Frame() - m_lastMinePlacement < MinePlacementTime()) return;
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}
*/

	{
		auto Threats5 = findThreats(Agent(), 5*32);
		bool enemyTargetedByMines = false;
		for (const auto * pFaceOff : Threats5)
			if (auto * pHisUnit = pFaceOff->His()->IsHisUnit())
				if (!pHisUnit->MinesTargetingThis().empty() && (pFaceOff->GroundDistanceToHitHim() < 6*32))
					enemyTargetedByMines = true;
		if (enemyTargetedByMines) return Agent()->ChangeBehavior<Fleeing>(Agent());
	}

	int minDist = numeric_limits<int>::max();
	HisUnit * pClosestTarget = nullptr;
	for (HisUnit * u : m_Targets)
	{
		int d = u->GetDistanceToTarget(Agent());
		if (d < minDist)
		{
			minDist = d;
			pClosestTarget = u;
		}
	}

	if (minDist < 48)
	{
		if (Agent()->RemainingMines() == 0) return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		m_lastMinePlacement = ai()->Frame();
	//	return Agent()->PlaceMine(Agent()->Pos());
		return Agent()->ChangeBehavior<Laying>(Agent());
	}

	assert_throw(pClosestTarget);
#if DEV
	drawLineMap(Agent()->Pos(), pClosestTarget->Pos(), Colors::Grey);
#endif

///	bw << Agent()->NameWithId() << " : " << minDist << endl;

	Vect AT = toVect(pClosestTarget->Pos() - Agent()->Pos());
	double divergingForceCoef = min(AT.Norm(), AT.Norm()*AT.Norm()/256);
	Vect divergingForce = CalcultateDivergingVector() * divergingForceCoef;
	Vect dir = AT + divergingForce;
#if DEV
	drawLineMap(Agent()->Pos(), Agent()->Pos() + toPosition(AT), Colors::Green, crop);//1
	drawLineMap(Agent()->Pos(), Agent()->Pos() + toPosition(divergingForce), Colors::Yellow, crop);//1
	drawLineMap(Agent()->Pos(), Agent()->Pos() + toPosition(dir), Colors::White, crop);//1
#endif
	Position dest = ai()->GetMap().Crop(Agent()->Pos() + toPosition(dir));
	Agent()->Patrol(dest);
}


} // namespace iron



