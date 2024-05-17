//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "dropping1T.h"
#include "dropping.h"
#include "defaultBehavior.h"
#include "../units/factory.h"
#include "../units/starport.h"
#include "../units/cc.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{





bool Dropping1T::EnterCondition(MyUnit * u)
{
	if (My<Terran_Dropship> * pDropship = u->IsMy<Terran_Dropship>())
		if (pDropship->GetTank())
			if (pDropship->CanUnload())
			//if (pDropship->Acceleration().Norm() > pDropship->Speed()*3/4)
			{
		//		int minDist = numeric_limits<int>::max();
				HisUnit * pTarget = nullptr;

				for (const auto & target : him().Units())
					if (!target->InFog() && target->Unit()->exists())
						if (roundedDist(target->Pos(), u->Pos()) < 10*32)
						{
							int targetRange = max(target->GroundRange(), target->AirRange());


							if (u->GetDistanceToTarget(target.get()) < 32*7+32)
							{
								pTarget = target.get();
								if (target->GetDistanceToTarget(u) < targetRange)
									return false;
							}
						}

				if (pTarget) return true;

			}
	return false;
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Dropping1T
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Dropping1T *> Dropping1T::m_Instances;

Dropping1T::Dropping1T(My<Terran_Dropship> * pAgent)
	: Behavior(pAgent, behavior_t::Dropping1T)
{
	assert_throw(Agent()->Is(Terran_Dropship));

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

	m_inStateSince = ai()->Frame();

///	ai()->SetDelay(100);
}


Dropping1T::~Dropping1T()
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


void Dropping1T::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string Dropping1T::StateName() const
{CI(this);
	switch(State())
	{
	case entering:		return "entering";
	case landing:		return "landing";
	default:			return "?";
	}
}



My<Terran_Dropship> * Dropping1T::ThisDropship() const
{CI(this);
	return Agent()->IsMy<Terran_Dropship>();
}

My<Terran_Siege_Tank_Tank_Mode> * Dropping1T::GetTank() const
{/*
	for (BWAPIUnit * u : m_Units)
		if (My<Terran_Siege_Tank_Tank_Mode> * pTank = dynamic_cast<My<Terran_Siege_Tank_Tank_Mode> *>(u))
			return pTank;
			*/
	return nullptr;
}


vector<My<Terran_SCV> *> Dropping1T::GetSCVs() const
{
	vector<My<Terran_SCV> *> List;
	/*
	for (BWAPIUnit * u : m_Units)
		if (My<Terran_SCV> * pSCV = u->IsMyUnit()->IsMy<Terran_SCV>())
			List.push_back(pSCV);*/

	return List;
}


void Dropping1T::OnFrame_v()
{CI(this);
	if (!Agent()->CanAcceptCommand()) return;

	switch (State())
	{
	case entering:		OnFrame_entering(); break;
	case landing:		OnFrame_landing(); break;
	default: assert_throw(false);
	}
}




void Dropping1T::OnFrame_collecting()
{CI(this);/*
//	const bool inHisArea = (Agent()->GetArea() == HisBase()->BWEMPart()->GetArea());

	if (JustArrivedInState())
	{
//		assert_throw(!Agent()->Loaded());
		bw << StateName() << " " << ai()->Frame() << " " << ai()->Frame() - m_inStateSince << endl;
		m_inStateSince = ai()->Frame();
	}

	if (!GetTank())
	{
		if (My<Terran_Siege_Tank_Tank_Mode> * tank = FindTank())
		{
			if (dist(tank->Pos(), Agent()->Pos()) < 5*32)
			{
				tank->ChangeBehavior<Dropping>(tank);
				m_Units.push_back(tank);
			}
			else return Agent()->Move(tank->Pos());
		}
		else return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}
	
	if (!GetTank()->Loaded()) return ThisDropship()->Load(GetTank());

	if (dist(ai()->GetMap().Center(), Agent()->Pos()) < 5*32)
		return ChangeState(landing);

	return Agent()->Move(ai()->GetMap().Center());*/
}


void Dropping1T::OnFrame_entering()
{CI(this);
//	const bool inHisArea = (Agent()->GetArea() == HisBase()->BWEMPart()->GetArea());

	if (JustArrivedInState())
	{
//		assert_throw(!Agent()->Loaded());
		bw << StateName() << " " << ai()->Frame() << " " << ai()->Frame() - m_inStateSince << endl;
		m_inStateSince = ai()->Frame();
	}

	My<Terran_Siege_Tank_Tank_Mode> * tank = nullptr;
		for (const auto & u : me().Units(Terran_Siege_Tank_Tank_Mode))
			if (u->Completed())
				if (groundDist(u->Pos(), Agent()->Pos()) < 2*32)
					tank = u->As<Terran_Siege_Tank_Tank_Mode>();
	if (!tank) return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	if (!tank->Loaded()) return ThisDropship()->Load(tank);

	My<Terran_SCV> * scv = nullptr;
		for (const auto & u : me().Units(Terran_SCV))
			if (u->Completed())
				if (groundDist(u->Pos(), Agent()->Pos()) < 2*32)
					scv = u->As<Terran_SCV>();
	if (!scv) return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	if (!scv->Loaded()) return ThisDropship()->Load(scv);


	return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
//	return ChangeState(landing);

/*
	if (ai()->Frame() - m_inStateSince > 15)
		if (Agent()->Unit()->isIdle())
		{
			bw << Agent()->NameWithId() << " was idle" << endl;
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		}
*/
}


void Dropping1T::OnFrame_landing()
{CI(this);
	if (JustArrivedInState())
	{
//		assert_throw(Agent()->Loaded());
		bw << StateName() << " " << ai()->Frame() << " " << ai()->Frame() - m_inStateSince << endl;
		m_inStateSince = ai()->Frame();
	}

	My<Terran_Siege_Tank_Tank_Mode> * tank = ThisDropship()->GetTank();
	if (!tank)
		for (const auto & u : me().Units(Terran_Siege_Tank_Tank_Mode))
			if (u->Completed())
				if (groundDist(u->Pos(), Agent()->Pos()) < 2*32)
					tank = u->As<Terran_Siege_Tank_Tank_Mode>();
					
	if (!tank) return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

	if (!tank->Loaded())
	{
			if (tank->CanAcceptCommand())
				tank->HoldPosition();/*
		if (!tank->CoolDown())
		{
			if (tank->CanAcceptCommand())
				tank->HoldPosition();
			return;
/*
			for (const auto & b : me().Buildings(Terran_Factory))
			{
				if (tank->CanAcceptCommand())
					tank->Attack(b.get());
				return;
			}
*//*
		}
		else*/
		return ChangeState(entering);
	//	return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}

	if (tank->Loaded())
		if (ai()->Frame() - m_inStateSince >= 7)
		{
			if (!ThisDropship()->CanUnload())
				return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
			
			return ThisDropship()->Unload(tank);
			//return ThisDropship()->UnloadAll();
		}

/*
//	if (!ai()->GetStrategy()->Detected<ZerglingRush>() || (m_pDropship->LoadedUnits() >= 2))
		if (ai()->Frame() - m_inStateSince > 15)
			if (Agent()->NotFiringFor() > 10)
				if (findThreats(Agent(), 2*32).empty())
					return m_pDropship->Unload(Agent());
*/
}



/*
{
	if (!Agent()->CanAcceptCommand()) return;

	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
	}


	frame_t framesSinceLastMove = ai()->Frame() - m_lastMove;
	//if (framesSinceLastMove < 10) return;*/
/*
	Vect dir;
	
	do dir = Vect(rand()%200 - 100, rand()%200 - 100);
	while(dir.Norm() < 10);

	dir.Normalize();

	dir *= 32*8;
	Position delta = toPosition(dir);
*/
/*
	Position delta(32*8, 0);
	if (Agent()->Pos().x > ai()->GetMap().Size().x*32 / 2) delta.x = -delta.x;
	if (ai()->Frame() - m_inStateSince > 200) delta.x = -delta.x;

	Position target = Agent()->Pos() + delta;
	drawLineMap(Agent()->Pos(), target, Colors::White, true);
	//delta.x = -delta.x;

	//bw << framesSinceLastMove << endl;
	m_lastMove = ai()->Frame();
	Agent()->Move(target);
*/
//}



} // namespace iron



