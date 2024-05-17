//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "guarding.h"
#include "walking.h"
#include "repairing.h"
#include "sieging.h"
#include "kiting.h"
#include "razing.h"
#include "raiding.h"
#include "defaultBehavior.h"
#include "../units/factory.h"
#include "../strategy/strategy.h"
#include "../strategy/baseDefense.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Guarding
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Guarding *> Guarding::m_Instances;


Guarding::Guarding(MyUnit * pAgent, const Base * pWhere)
	: Behavior(pAgent, behavior_t::Guarding), m_pWhere(pWhere)
{
	assert_throw(pAgent->Flying() || pWhere->GetArea()->AccessibleFrom(pAgent->GetArea()));

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

	m_inStateSince = ai()->Frame();

	const int guardians = count_if(Instances().begin(), Instances().end(),
		[this](const Guarding * g){ return g->Where() == Where(); });

	m_target = (guardians == 1)
		? center(ext(Where())->GuardingCorners().first)
		: center(ext(Where())->GuardingCorners().second);

}



Guarding::~Guarding()
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


void Guarding::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string Guarding::StateName() const
{CI(this);
	switch(State())
	{
	case reachingArea:		return "reachingArea";
	case guarding:			return "guarding";
	default:				return "?";
	}
}


void Guarding::OnFrame_v()
{CI(this);
#if DEV
	if ((State() != reachingArea) && (m_target != Positions::None)) drawLineMap(Agent()->Pos(), m_target, GetColor());//1
#endif


	if (!Agent()->CanAcceptCommand()) return;

	if (auto pTank = Agent()->IsMy<Terran_Siege_Tank_Tank_Mode>())
		if (Sieging::EnterCondition(pTank))
			return pTank->ChangeBehavior<Sieging>(pTank);

	if (Kiting::KiteCondition(Agent()) || Kiting::AttackCondition(Agent()))
		return Agent()->ChangeBehavior<Kiting>(Agent());

	if (Agent()->Type().isMechanical())
		if (Agent()->Life() < Agent()->MaxLife())
			if (Agent()->RepairersCount() < Agent()->MaxRepairers())
				if (Repairing * pRepairer = Repairing::GetRepairer(Agent(),
							(Agent()->Life()*4 > Agent()->MaxLife()*3) ? 16*32 :
							(Agent()->Life()*4 > Agent()->MaxLife()*2) ? 32*32 :
							(Agent()->Life()*4 > Agent()->MaxLife()*1) ? 64*32 : 1000000))
					return Agent()->ChangeBehavior<Repairing>(Agent(), pRepairer);

	switch (State())
	{
	case reachingArea:	OnFrame_reachingArea(); break;
	case guarding:		OnFrame_guarding(); break;
	default: assert_throw(false);
	}
}


void Guarding::OnFrame_reachingArea()
{CI(this);
//	const bool inHisArea = (Agent()->GetArea() == HisBase()->BWEMPart()->GetArea());

	if (Agent()->GetArea(check_t::no_check) == Where()->GetArea())
	{
		ResetSubBehavior();
		return ChangeState(guarding);
	}

	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
		return SetSubBehavior<Walking>(Agent(), Where()->Center(), __FILE__ + to_string(__LINE__));
	}

	if (ai()->Frame() - m_inStateSince > 5)
		if (Agent()->Unit()->isIdle())
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
}


void Guarding::OnFrame_guarding()
{CI(this);
	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
		return Agent()->Move(m_target);
	}

	if (Razing::Condition(Agent()))
		return Agent()->ChangeBehavior<Razing>(Agent(), Agent()->FindArea());

	if (auto s = ai()->GetStrategy()->Active<BaseDefense>())
		for (const Area * attackedArea : s->AttackedBase()->GetArea()->EnlargedArea())
		for (const Area * guardedArea : ext(Where()->GetArea())->EnlargedArea())
			if (contains(guardedArea->AccessibleNeighbours(), attackedArea))
			{
			///	ai()->SetDelay(500);
			///	bw << Agent() << " go to BaseDefense" << endl;
				return Agent()->ChangeBehavior<Raiding>(Agent(), s->AttackedBase()->BWEMPart()->Center());
			}
}



} // namespace iron



