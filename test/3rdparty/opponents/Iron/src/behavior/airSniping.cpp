//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "airSniping.h"
#include "fleeing.h"
#include "defaultBehavior.h"
#include "../units/starport.h"
#include "../strategy/strategy.h"
#include "../strategy/zerglingRush.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


My<Terran_Dropship> * AirSniping::FindDropship(MyBWAPIUnit * u, int maxDist)
{
	My<Terran_Dropship> * pClosestDropship = nullptr;

	if (u->Is(Terran_SCV) || u->Is(Terran_Siege_Tank_Tank_Mode))
		for (const unique_ptr<MyUnit> & v : me().Units(Terran_Dropship))
			if (My<Terran_Dropship> * pDropship = v->IsMy<Terran_Dropship>())
				if (v->Completed())
					//if (pDropship->Snipers() < 4)
						if (roundedDist(u->Pos(), v->Pos()) < maxDist)
						{
							maxDist = roundedDist(u->Pos(), v->Pos());
							pClosestDropship = pDropship;
						}

	return pClosestDropship;
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class AirSniping
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<AirSniping *> AirSniping::m_Instances;


AirSniping::AirSniping(MyUnit * pAgent, My<Terran_Dropship> * pDropship)
	: Behavior(pAgent, behavior_t::AirSniping), m_pDropship(pDropship)
{
	assert_throw(pAgent->Is(Terran_SCV) || pAgent->Is(Terran_Siege_Tank_Tank_Mode));
//	assert_throw(m_pDropship->LoadedUnits() < 4);

//	assert_throw(!pAgent->Loaded());
//	if (pAgent->Loaded()) bw << pAgent->NameWithId() << " already loaded!" << endl;	TODO

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

	m_inStateSince = ai()->Frame();
}


AirSniping::~AirSniping()
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


void AirSniping::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string AirSniping::StateName() const
{CI(this);
	switch(State())
	{
	case entering:		return "entering";
	case landing:		return "landing";
	default:			return "?";
	}
}


void AirSniping::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);
	if (m_pDropship == other)
	{
		m_pDropship = nullptr;
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}
}

void AirSniping::OnFrame_v()
{CI(this);
#if DEV
	drawLineMap(Agent()->Pos(), m_pDropship->Pos(), GetColor());
#endif

//	if (!Agent()->CanAcceptCommand()) return;
	if (!Dropship()->CanAcceptCommand()) return;

	switch (State())
	{
	case entering:		OnFrame_entering(); break;
	case landing:		OnFrame_landing(); break;
	default: assert_throw(false);
	}
}


void AirSniping::OnFrame_entering()
{CI(this);
//	const bool inHisArea = (Agent()->GetArea() == HisBase()->BWEMPart()->GetArea());

	if (JustArrivedInState())
	{
//		assert_throw(!Agent()->Loaded());
		bw << StateName() << " " << ai()->Frame() << " " << ai()->Frame() - m_inStateSince << endl;
		m_inStateSince = ai()->Frame();
	}

	if (Agent()->Loaded())
	{
		return ChangeState(landing);
	}

	if (!Agent()->Loaded()) return m_pDropship->Load(Agent());

/*
	if (ai()->Frame() - m_inStateSince > 15)
		if (Agent()->Unit()->isIdle())
		{
			bw << Agent()->NameWithId() << " was idle" << endl;
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		}
*/
}


void AirSniping::OnFrame_landing()
{CI(this);
	if (JustArrivedInState())
	{
//		assert_throw(Agent()->Loaded());
		bw << StateName() << " " << ai()->Frame() << " " << ai()->Frame() - m_inStateSince << endl;
		m_inStateSince = ai()->Frame();
	}

	if (!Agent()->Loaded())
	{
		if (!Agent()->CoolDown())
			for (const auto & b : me().Buildings(Terran_Factory))
			{
				if (Agent()->CanAcceptCommand())
					Agent()->Attack(b.get());
				return;
			}
		else
		return ChangeState(entering);
	//	return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}

	if (Agent()->Loaded())
		if (ai()->Frame() - m_inStateSince >= 7)
			return m_pDropship->Unload(Agent());

/*
//	if (!ai()->GetStrategy()->Detected<ZerglingRush>() || (m_pDropship->LoadedUnits() >= 2))
		if (ai()->Frame() - m_inStateSince > 15)
			if (Agent()->NotFiringFor() > 10)
				if (findThreats(Agent(), 2*32).empty())
					return m_pDropship->Unload(Agent());
*/
}




} // namespace iron



