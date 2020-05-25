//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "executing.h"
#include "defaultBehavior.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Executing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Executing *> Executing::m_Instances;

Executing::Executing(MyUnit * pAgent)
	: Behavior(pAgent, behavior_t::Executing)
{

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

///	ai()->SetDelay(100);
}


Executing::~Executing()
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


void Executing::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


void Executing::OnFrame_v()
{CI(this);
	if (!Agent()->CanAcceptCommand()) return;

	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
	}
/*
	if (Agent()->CoolDown()) return;

	multimap<int, MyUnit *> Targets;
	for (const auto & u : me().Units(Terran_Vulture_Spider_Mine))
		if (!u->Burrowed())
			if (Agent()->GetDistanceToTarget(u.get()) < Agent()->GroundRange())
				Targets.emplace(u->Life(), u.get());

	if (!Targets.empty())
		Agent()->Attack(Targets.begin()->second, check_t::no_check);
*/

/*
	multimap<int, MyUnit *> Targets;
	for (const auto & u : me().Units(Terran_Vulture))
		if (u.get() != Agent())
			Targets.emplace(groundDist(u->Pos(), Agent()->Pos()), u.get());

	if (Targets.empty()) return;

	MyUnit * pTarget = Targets.begin()->second;

	//if (Agent()->CoolDown() == 0)
	if (m_pLastAttackedTarget != pTarget)
	{
		m_lastAttack = ai()->Frame();
		bw->drawCircleMap(pTarget->Pos(), 18, Colors::Red, true);
		m_pLastAttackedTarget = pTarget;
		Agent()->Attack(pTarget, check_t::no_check);
	}
*/
}



} // namespace iron



