//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "walking.h"
#include "defaultBehavior.h"
#include "../strategy/strategy.h"
#include "../strategy/earlyRunBy.h"
#include "../units/factory.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Walking
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


Walking::Walking(MyBWAPIUnit * pAgent, Position target, const string & callerFileAndLine)
	: Behavior(pAgent, behavior_t::Walking), m_target(target), m_Path(ai()->GetMap().GetPath(pAgent->Pos(), target, &m_length))
{
	assert_throw_plus(m_length >= 0, my_to_string(pAgent->Pos()) + my_to_string(target) + "from " + callerFileAndLine);

}


Walking::~Walking()
{
}


string Walking::StateName() const
{CI(this);
	switch(State())
	{
	case intermediate:	return "intermediate";
	case final:			return "final";
	case failed:		return "failed";
	case succeeded:		return "succeeded";
	default:			return "?";
	}
}


Position Walking::CurrentWayPoint() const
{CI(this);
	if (m_index == (int)m_Path.size()) return Target();
	
	return Position(m_Path[m_index]->Center());
}


void Walking::OnFrame_v()
{CI(this);
#if DEV
	drawLineMap(Agent()->Pos(), Target(), GetColor());//1
	drawLineMap(Agent()->Pos(), CurrentWayPoint(), Colors::Grey);//1
#endif

	if (!Agent()->CanAcceptCommand()) return;

	if (Agent()->Unit()->canUnsiege())
	{
	///	bw << Agent()->NameWithId() << " unsieged first" << endl;
		return static_cast<My<Terran_Siege_Tank_Tank_Mode> *>(Agent())->Unsiege();
	}
	if (Agent()->Unit()->isSieged()) return;

	switch (State())
	{
	case intermediate:		OnFrame_intermediate(); break;
	case final:				OnFrame_final(); break;
	case failed:			break;
	case succeeded:			break;
	default: assert_throw(false);
	}
}


bool Walking::ShouldPatrol() const
{
	return false;

	if (auto s = ai()->GetStrategy()->Active<EarlyRunBy>())
		if (s->InSquad(Agent()->IsMyUnit()))
			return true;

	return false;
}


void Walking::OnFrame_intermediate()
{CI(this);
	Position wp = CurrentWayPoint();

	if (wp == Target()) return ChangeState(final);

	if (Agent()->Pos().getApproxDistance(wp) < 32*10)
	{
		++m_index;
		if (ShouldPatrol())	return Agent()->Patrol(CurrentWayPoint());
		else				return Agent()->Move(CurrentWayPoint());
	}

	if (JustArrivedInState())
		if (ShouldPatrol())	return Agent()->Patrol(wp);
		else				return Agent()->Move(wp);

	if (Agent()->Unit()->isIdle())
	{
		if (++m_tries >= 10)
			return ChangeState(failed);
		
	///	bw << Agent()->NameWithId() << ", move!" << endl;
		if (ShouldPatrol())	return Agent()->Patrol(wp);
		else				return Agent()->Move(wp);
	}

}


void Walking::OnFrame_final()
{CI(this);
	if (Agent()->Pos().getApproxDistance(Target()) < m_targetRadius)
		return ChangeState(succeeded);

	if (JustArrivedInState())
	{
		m_tries = 0;
		if (ShouldPatrol())	return Agent()->Patrol(Target());
		else				return Agent()->Move(Target());
	}

	if (m_tries >= 3)
	{
#if DEV
		bw->drawCircleMap(Agent()->Pos(), m_targetRadius, Colors::Grey);//1
#endif
		m_targetRadius += 5;
	}


	if (Agent()->Unit()->isIdle())
	{
		++m_tries;
	///	bw << Agent()->NameWithId() << ", move!" << endl;
		if (ShouldPatrol())	return Agent()->Patrol(Target());
		else				return Agent()->Move(Target());
	}

}



} // namespace iron



