//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "blocking.h"
#include "walking.h"
#include "fleeing.h"
#include "mining.h"
#include "defaultBehavior.h"
#include "../territory/stronghold.h"
#include "../units/cc.h"
#include "../strategy/strategy.h"
#include "../strategy/zerglingRush.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Blocking
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Blocking *> Blocking::m_Instances;


Blocking::Blocking(MyUnit * pAgent, VChokePoint * pWhere)
	: Behavior(pAgent, behavior_t::Blocking), m_pWhere(pWhere)
{
	assert_throw(!pAgent->Flying());
	assert_throw(!pWhere->BWEMPart()->Blocked());
	assert_throw(pWhere->BWEMPart()->GetAreas().first->AccessibleFrom(pAgent->GetArea()));

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

	m_inStateSince = ai()->Frame();
}


Blocking::~Blocking()
{CI(this);
#if !DEV
	try //3
#endif
	{
		assert_throw(contains(m_Instances, this));
		really_remove(m_Instances, this);

		for (Blocking * pBlocking : m_Instances)
			if (pBlocking->m_pRepaired == this)
				pBlocking->m_pRepaired = nullptr;
	}
#if !DEV
	catch(...){} //3
#endif
}


void Blocking::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string Blocking::StateName() const
{CI(this);
	switch(State())
	{
	case reachingCP:	return "reachingCP";
	case blocking:		return "blocking";
	case dragOut:		return "dragOut";
	default:			return "?";
	}
}


void Blocking::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);
	if (m_pTarget == other)
		m_pTarget = nullptr;
}


bool Blocking::CanRepair(const MyBWAPIUnit * pTarget, int) const
{
	return pTarget->Is(Terran_Bunker) && pTarget->Life() < pTarget->MaxLife();
}


void Blocking::OnFrame_v()
{CI(this);
#if DEV
	if (State() == reachingCP)
		drawLineMap(Agent()->Pos(), center(m_pWhere->BWEMPart()->Center()), GetColor());
	else if (State() == blocking)
	{
		if (m_pTarget) drawLineMap(Agent()->Pos(), m_pTarget->Pos(), GetColor());
		if (m_pRepaired) drawLineMap(Agent()->Pos(), m_pRepaired->Agent()->Pos(), GetColor());
	}
#endif


	if (!Agent()->CanAcceptCommand()) return;

	if (!ai()->GetStrategy()->Detected<ZerglingRush>())
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

	switch (State())
	{
	case reachingCP:	OnFrame_reachingCP(); break;
	case blocking:		OnFrame_blocking(); break;
	case dragOut:		OnFrame_dragOut(); break;
	default: assert_throw(false);
	}
}


void Blocking::OnFrame_reachingCP()
{CI(this);
//	const bool inHisArea = (Agent()->GetArea() == HisBase()->BWEMPart()->GetArea());

	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
		return SetSubBehavior<Walking>(Agent(), center(Where()->BWEMPart()->Center()), __FILE__ + to_string(__LINE__));
	}

	if (Walking * pWalking = GetSubBehavior()->IsWalking())
	{
//		if ((pWalking->State() == Walking::succeeded) ||
//			((Agent()->PrevPos(0) == Agent()->PrevPos(1)) && (dist(Agent()->Pos(), pWalking->Target()) < 4*32)))
		if (dist(Agent()->Pos(), pWalking->Target()) < 10*32)
			{
				ResetSubBehavior();
				return ChangeState(blocking);
			}
	}

	if (Agent()->IsMy<Terran_SCV>())
	{
		My<Terran_SCV> * pCloserWorker = findFreeWorker(Agent()->GetStronghold()->HasBase(), [this](const My<Terran_SCV> * u)
					{ return (u->Life() > 50) && dist(u->Pos(), center(Where()->BWEMPart()->Center())) < dist(Agent()->Pos(), center(Where()->BWEMPart()->Center()) - 5); });

		if (pCloserWorker)
		{
			pCloserWorker->ChangeBehavior<Blocking>(pCloserWorker, Where());
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		}
	}


	if (ai()->Frame() - m_inStateSince > 10)
		if (Agent()->Unit()->isIdle())
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
}


void Blocking::OnFrame_blocking()
{CI(this);
	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
	}

	m_pTarget = nullptr;
	m_pRepaired = nullptr;

	static int maxDragout = 5;
	if (Agent()->Is(Terran_SCV))
		if (Agent()->Life() < Agent()->PrevLife(10))
			if (Agent()->Life() <=
					10 +
					Agent()->PrevLife(15) - Agent()->Life() +
					10*me().CompletedBuildings(Terran_Bunker) +
					Agent()->CoolDown())
				if ((maxDragout-- >= 0) || (me().CompletedBuildings(Terran_Bunker) >= 1))
				{
				///	bw << Agent()->NameWithId() << " dragout ! " << maxDragout << endl;
					for (Blocking * pBlocking : m_Instances)
						if (pBlocking->m_pRepaired == this)
							pBlocking->m_pRepaired = nullptr;

					return ChangeState(dragOut);
				}



//	if (roundedDist(Agent()->Pos(), center(Where()->BWEMPart()->Center())) > 4*32 + 5)
//		return Agent()->Move(center(Where()->BWEMPart()->Center()));

	multimap<int, HisUnit *> Targets;
	for (const auto & faceOff : Agent()->FaceOffs())
		if (HisUnit * pHisUnit = faceOff.His()->IsHisUnit())
			if (pHisUnit->GroundRange() <= 32)
			{
			//	bool targetInside = pHisUnit->GetArea() && ext(pHisUnit->GetArea())->UnderDefenseCP();
				if (roundedDist(Agent()->Pos(), pHisUnit->Pos()) < (me().CompletedBuildings(Terran_Bunker) ? 40 : 70))
					Targets.emplace(faceOff.FramesToKillHim() + 10*roundedDist(Agent()->Pos(), pHisUnit->Pos()), pHisUnit);
			}

	m_pTarget = Targets.empty() ? nullptr : Targets.begin()->second;
	if (m_pTarget)
	{
		Agent()->Attack(m_pTarget);
		return;
	}

	if (me().CompletedBuildings(Terran_Bunker) >= 1)
		if (ai()->Frame() % 60 < 30)
		{
			int minDistToBunker = numeric_limits<int>::max();
			MyBuilding * pClosestBunker = nullptr;
			for (const auto & b : me().Buildings(Terran_Bunker))
				if (b->Completed())
				{
					int d = distToRectangle(Agent()->Pos(), b->TopLeft(), b->Size());
					if (d < minDistToBunker)
					{
						minDistToBunker = d;
						pClosestBunker = b.get();
					}
				}

			if (pClosestBunker && (minDistToBunker > 0))
				return Agent()->Move(pClosestBunker->Pos());
		}

	if (auto * pSCV = Agent()->IsMy<Terran_SCV>())
	{
		int repairers = 0;
		int minDistance = numeric_limits<int>::max();
		Blocking * pNearestHarmed = nullptr;
		for (Blocking * pBlocking : Blocking::Instances())
			if (pBlocking->Where() == Where())
				if (pBlocking != this)
					if (pBlocking->State() != dragOut)
					{
						if (pBlocking->m_pRepaired) ++repairers;

						if (pBlocking->Agent()->Life() < pBlocking->Agent()->MaxLife())
						{
							int d = roundedDist(Agent()->Pos(), pBlocking->Agent()->Pos());
							if (!pBlocking->Target()) d += 10000;

							if (d < minDistance)
							{
								minDistance = d;
								pNearestHarmed = pBlocking;
							}
						}
					}

		if (pNearestHarmed)
			if (repairers < 4)
				if ((me().Buildings(Terran_Bunker).size() >= 1) || (me().MineralsAvailable() > Cost(Terran_Bunker).Minerals() + 20))
				if (!Agent()->FaceOffs().empty() || (me().Units(Terran_Marine).size() >= 2))
				if (Mining::Instances().size() >= 4)
				{
					int dist = roundedDist(Agent()->Pos(), pNearestHarmed->Agent()->Pos());
					int prevDist = roundedDist(Agent()->PrevPos(1), pNearestHarmed->Agent()->Pos());
					if ((dist > 48) && (prevDist > dist))
					{

					}
					else
					{
						m_pRepaired = pNearestHarmed;
						return pSCV->Repair(m_pRepaired->Agent());
					}
				}
	}

//	Agent()->Stop();
}


void Blocking::OnFrame_dragOut()
{CI(this);
	if (JustArrivedInState())
	{
		assert_throw(Agent()->Is(Terran_SCV));

		m_inStateSince = ai()->Frame();

		for (Mineral * m : me().StartingBase()->Minerals())
			if (m->Unit()->isVisible())
				return Agent()->IsMy<Terran_SCV>()->Gather(m);

		return Agent()->ChangeBehavior<Fleeing>(Agent());
	}

	if (ai()->Frame() - m_inStateSince > 50)
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
}




} // namespace iron



