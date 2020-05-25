//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "patrollingBases.h"
#include "walking.h"
#include "repairing.h"
#include "sieging.h"
#include "kiting.h"
#include "defaultBehavior.h"
#include "../units/factory.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class PatrollingBases
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<PatrollingBases *> PatrollingBases::m_Instances;
int PatrollingBases::m_instancesCreated = 0;


vector<const VBase *> PatrollingBases::Targets()
{
	vector<const VBase *> List;
	if (him().GetArea())
		for (const VBase & base : ai()->GetVMap().Bases())
			if (!contains(me().Bases(), &base))
				if (him().StartingVBase() != &base)
					if (base.BWEMPart()->GetArea()->AccessibleFrom(him().GetArea()))
						List.push_back(&base);

	return List;
}


PatrollingBases * PatrollingBases::GetPatroller()
{
	if (!him().GetArea()) return nullptr;
	if (Instances().size() >= Targets().size()) return nullptr;

	int targetsToBlock = 0;
	for (const VBase * base : PatrollingBases::Targets())
		if (!base->BlockedByOneOfMyMines())
			++targetsToBlock;


	My<Terran_Vulture> * pBestCandidate = nullptr;

	for (const auto & u : me().Units(Terran_Vulture))
	{
		My<Terran_Vulture> * pVulture = u->IsMy<Terran_Vulture>();
		if (u->Completed())
			if (!u->Loaded())
				if (u->GetArea()->AccessibleFrom(him().GetArea()))
					if (u->GetBehavior()->IsRaiding() ||
						u->GetBehavior()->IsExploring())
						if (u->Life() == u->MaxLife())
							if (!pBestCandidate ||
								(targetsToBlock == 0
									? (pVulture->RemainingMines() < pBestCandidate->RemainingMines())
									: (pVulture->RemainingMines() > pBestCandidate->RemainingMines())
									)
								)
								pBestCandidate = pVulture;
	}

	if (!pBestCandidate) return nullptr;
	
	pBestCandidate->ChangeBehavior<PatrollingBases>(pBestCandidate->IsMy<Terran_Vulture>());
	return pBestCandidate->GetBehavior()->IsPatrollingBases();
}


PatrollingBases::PatrollingBases(My<Terran_Vulture> * pAgent)
	: Behavior(pAgent, behavior_t::PatrollingBases)
{
	m_pTarget = FindFirstTarget();
	assert_throw(m_pTarget);
	m_Visited.push_back(m_pTarget);
	SetSubBehavior<Walking>(pAgent, m_pTarget->BWEMPart()->Center(), __FILE__ + to_string(__LINE__));
	m_pTarget->UpdateLastTimePatrolled();

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);
	++m_instancesCreated;
}


PatrollingBases::~PatrollingBases()
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


const VBase * PatrollingBases::FindFirstTarget() const
{CI(this);
	const VBase * pOldestTarget = nullptr;
	frame_t minLastTimePatrolled = numeric_limits<int>::max();

	for (const VBase * base : PatrollingBases::Targets())
		if (!contains(m_Visited, base))
		{
			frame_t lastTimePatrolled = base->LastTimePatrolled();
			if (lastTimePatrolled < minLastTimePatrolled)
			{
				pOldestTarget = base;
				minLastTimePatrolled = lastTimePatrolled;
			}
		}

	return pOldestTarget;
}


const VBase * PatrollingBases::FindNextTarget() const
{CI(this);
	const VBase * pNearestTarget = nullptr;
	int minDist = numeric_limits<int>::max();

	for (const VBase * base : PatrollingBases::Targets())
		if (!contains(m_Visited, base))
		{
			int d = roundedDist(Agent()->Pos(), base->BWEMPart()->Center());
			if (d < minDist)
			{
				pNearestTarget = base;
				minDist = d;
			}
		}

	return pNearestTarget;
}


void PatrollingBases::OnFrame_v()
{CI(this);
//	if (!Agent()->CanAcceptCommand()) return;

#if DEV
	drawLineMap(Agent()->Pos(), Target()->BWEMPart()->Center(), GetColor());//1
#endif

	if (contains(me().Bases(), Target())) return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

	if (!Agent()->CanAcceptCommand()) return;

	if (Agent()->Life() < Agent()->PrevLife(10))
		return Agent()->ChangeBehavior<Kiting>(Agent());

	if (Sieging::EnterCondition(Agent()))
		return Agent()->ChangeBehavior<Sieging>(Agent()->As<Terran_Siege_Tank_Tank_Mode>());

	if (Kiting::KiteCondition(Agent()) || Kiting::AttackCondition(Agent()))
		return Agent()->ChangeBehavior<Kiting>(Agent());

/*
	if (Agent()->Life() < Agent()->MaxLife())
		if (Agent()->Type().isMechanical())
			if (Agent()->RepairersCount() < Agent()->MaxRepairers())
				if (Repairing * pRepairer = Repairing::GetRepairer(Agent(),
					Agent()->StayInArea() ? 8*32 :
					(Agent()->Life()*4 > Agent()->MaxLife()*3) ? 10*32 :
					(Agent()->Life()*4 > Agent()->MaxLife()*2) ? 30*32 : 1000000))
				{
					return Agent()->ChangeBehavior<Repairing>(Agent(), pRepairer);
				}
*/

	if (ai()->Frame() - m_lastMinePlacement < 50) return;


	if (dist(Agent()->Pos(), Position(m_pTarget->BWEMPart()->Center())) < 32*3)
	{
		if (me().HasResearched(TechTypes::Spider_Mines))
			if (!m_pTarget->BlockedByOneOfMyMines())
				if (Agent()->RemainingMines() >= 1)
				{
				///	ai()->SetDelay(500);
					m_lastMinePlacement = ai()->Frame();
					return Agent()->PlaceMine(m_pTarget->BWEMPart()->Center());
				}


		m_pTarget = FindNextTarget();
		if (!m_pTarget) return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

		m_Visited.push_back(m_pTarget);
		ResetSubBehavior();
		SetSubBehavior<Walking>(Agent(), m_pTarget->BWEMPart()->Center(), __FILE__ + to_string(__LINE__));
		m_pTarget->UpdateLastTimePatrolled();
	}
}



} // namespace iron



