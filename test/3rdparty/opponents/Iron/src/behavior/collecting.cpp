//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "collecting.h"
#include "dropping.h"
#include "defaultBehavior.h"
#include "../units/factory.h"
#include "../units/starport.h"
#include "../units/cc.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

const int nb_scv = 4;

bool Collecting::EnterCondition(My<Terran_Dropship> * pDropship)
{
	if (!pDropship->GetTank()) return true;

//	if (!pDropship->GetVulture()) return true;

	if (pDropship->GetSCVs().size() < nb_scv) return true;

	return false;
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Collecting
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Collecting *> Collecting::m_Instances;

Collecting::Collecting(My<Terran_Dropship> * pAgent)
	: Behavior(pAgent, behavior_t::Collecting)
{
	assert_throw(Agent()->Is(Terran_Dropship));

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

	m_initialLoadedUnits = pAgent->LoadedUnits().size();

///	ai()->SetDelay(100);
}


Collecting::~Collecting()
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


void Collecting::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string Collecting::StateName() const
{CI(this);
	switch(State())
	{
	case reachingTarget:return "reachingTarget";
	case collecting:	return "collecting";
	default:			return "?";
	}
}


My<Terran_Dropship> * Collecting::ThisDropship() const
{CI(this);
	return Agent()->IsMy<Terran_Dropship>();
}


MyUnit * Collecting::Find(UnitType type) const
{
	MyUnit * pBestCandidate = nullptr;
	int minDist = numeric_limits<int>::max();
	for (const auto & u : me().Units(type))
		if (u->Completed())
			if (!u->Loaded())
				if (!(u->GetBehavior()->IsSieging() || u->Unit()->isSieged()))
				{
					int d = roundedDist(u->Pos(), Agent()->Pos());
					if (d < minDist)
					{
						minDist = d;
						pBestCandidate = u.get();
					}
				}

	return pBestCandidate;
}


void Collecting::OnFrame_v()
{CI(this);
	if (!Agent()->CanAcceptCommand()) return;

	switch (State())
	{
	case reachingTarget:	OnFrame_reachingTarget(); break;
	case collecting:		OnFrame_collecting(); break;
	default: assert_throw(false);
	}
}


void Collecting::OnFrame_reachingTarget()
{CI(this);
//	const bool inHisArea = (Agent()->GetArea() == HisBase()->BWEMPart()->GetArea());

	if (JustArrivedInState())
	{
//		assert_throw(!Agent()->Loaded());
	///	bw << StateName() << " " << ai()->Frame() << " " << ai()->Frame() - m_inStateSince << endl;
		m_inStateSince = ai()->Frame();
	}

//	if (ThisDropship()->GetTank()) return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

	for (UnitType type : {Terran_Siege_Tank_Tank_Mode, Terran_Vulture, Terran_SCV})
		if (ThisDropship()->GetCargoCount(type) < (type == Terran_SCV ? nb_scv : type == Terran_Vulture ? 0 : 1))
		{
			MyUnit * u = Find(type);
			if (!u) return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

			if (dist(u->Pos(), Agent()->Pos()) > 10*32)
				return Agent()->Move(u->Pos());

			u->ChangeBehavior<Dropping>(u);
			ThisDropship()->Load(u);
			return ChangeState(collecting);
		}
}


void Collecting::OnFrame_collecting()
{CI(this);
//	const bool inHisArea = (Agent()->GetArea() == HisBase()->BWEMPart()->GetArea());

	if (JustArrivedInState())
	{
//		assert_throw(!Agent()->Loaded());
	///	bw << StateName() << " " << ai()->Frame() << " " << ai()->Frame() - m_inStateSince << endl;
		m_inStateSince = ai()->Frame();
	}

	if (m_initialLoadedUnits < (int)ThisDropship()->LoadedUnits().size())
	{
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}

	if (ai()->Frame() - m_inStateSince > 50)
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
}


} // namespace iron



