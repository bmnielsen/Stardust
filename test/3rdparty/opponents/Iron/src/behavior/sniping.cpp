//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "sniping.h"
#include "fleeing.h"
#include "defaultBehavior.h"
#include "../units/bunker.h"
#include "../strategy/strategy.h"
#include "../strategy/zerglingRush.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


My<Terran_Bunker> * Sniping::FindBunker(MyBWAPIUnit * u, int maxDist)
{
	My<Terran_Bunker> * pClosestBunker = nullptr;

	if (u->Is(Terran_SCV) || u->Is(Terran_Marine) || u->Is(Terran_Medic) || u->Is(Terran_Ghost))
		for (const unique_ptr<MyBuilding> & b : me().Buildings(Terran_Bunker))
			if (My<Terran_Bunker> * pBunker = b->IsMy<Terran_Bunker>())
				if (b->Completed())
					if (pBunker->Snipers() < 4)
						if (groundDist(u->Pos(), b->Pos()) < maxDist)
						{
							maxDist = groundDist(u->Pos(), b->Pos());
							pClosestBunker = pBunker;
						}

	return pClosestBunker;
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Sniping
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Sniping *> Sniping::m_Instances;


Sniping::Sniping(MyUnit * pAgent, My<Terran_Bunker> * pWhere)
	: Behavior(pAgent, behavior_t::Sniping), m_pWhere(pWhere)
{
	assert_throw(pAgent->Is(Terran_SCV) || pAgent->Is(Terran_Marine) || pAgent->Is(Terran_Medic) || pAgent->Is(Terran_Ghost));
	assert_throw(m_pWhere->LoadedUnits() < 4);

//	assert_throw(!pAgent->Loaded());
//	if (pAgent->Loaded()) bw << pAgent->NameWithId() << " already loaded!" << endl;	TODO

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

	m_inStateSince = ai()->Frame();
}


Sniping::~Sniping()
{CI(this);
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


void Sniping::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string Sniping::StateName() const
{CI(this);
	switch(State())
	{
	case entering:		return "entering";
	case sniping:		return "sniping";
	default:			return "?";
	}
}


void Sniping::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);
	if (m_pWhere == other)
	{
		m_pWhere = nullptr;
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}
}

void Sniping::OnFrame_v()
{CI(this);
#if DEV
	drawLineMap(Agent()->Pos(), m_pWhere->Pos(), GetColor());
#endif

	if (!Agent()->CanAcceptCommand()) return;
	if (!Where()->CanAcceptCommand()) return;

	switch (State())
	{
	case entering:		OnFrame_entering(); break;
	case sniping:		OnFrame_sniping(); break;
	default: assert_throw(false);
	}
}


void Sniping::OnFrame_entering()
{CI(this);
//	const bool inHisArea = (Agent()->GetArea() == HisBase()->BWEMPart()->GetArea());

	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
	//	assert_throw(!Agent()->Loaded());
		if (!Agent()->Loaded()) return m_pWhere->Load(Agent());
	}

	if (Agent()->Loaded())
	{
		return ChangeState(sniping);
	}

	if (ai()->Frame() - m_inStateSince > 15)
		if (Agent()->Unit()->isIdle())
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
}


void Sniping::OnFrame_sniping()
{CI(this);
	if (JustArrivedInState())
	{
		assert_throw(Agent()->Loaded());
		m_inStateSince = ai()->Frame();
	}

	if (!Agent()->Loaded())
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

//	if (!ai()->GetStrategy()->Detected<ZerglingRush>() || (m_pWhere->LoadedUnits() >= 2))
		if (ai()->Frame() - m_inStateSince > 15)
			if (Agent()->NotFiringFor() > 10)
				if (findThreats(Agent(), 2*32).empty())
					return m_pWhere->Unload(Agent());
}




} // namespace iron



