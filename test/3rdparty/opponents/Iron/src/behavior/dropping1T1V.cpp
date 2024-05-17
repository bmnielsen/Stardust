//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "dropping1T1V.h"
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


int Dropping1T1V::Targets(MyUnit * u)
{
	int dragoonsIn8 = 0;
	int dragoonsOut8 = 0;
	for (const auto & target : him().Units())
		if (!target->InFog() && target->Unit()->exists())
			if (roundedDist(target->Pos(), u->Pos()) < 10*32)
			{
				int targetRange = max(target->GroundRange(), target->AirRange());

				if (target->Is(Protoss_Dragoon))
				{
					if (u->GetDistanceToTarget(target.get()) < 32*8) ++dragoonsIn8;
					else ++dragoonsOut8;
				}
				else if (target->GetDistanceToTarget(u) - targetRange < 3*32) return 1000;
			}

	if (dragoonsIn8 == 0) return 0;
	return dragoonsIn8 + dragoonsOut8;
}


bool Dropping1T1V::EnterCondition(MyUnit * u)
{
	if (him().IsProtoss())
		if (My<Terran_Dropship> * pDropship = u->IsMy<Terran_Dropship>())
			if (pDropship->CanUnload())
				if (!Collecting::EnterCondition(pDropship))
				//if (pDropship->Acceleration().Norm() > pDropship->Speed()*3/4)
				{
			//		int minDist = numeric_limits<int>::max();

					int targets = Targets(u);
					auto damage = pDropship->GetDamage();

					if ((targets == 1) && (damage != My<Terran_Dropship>::damage_t::require_repair))
						return true;

					if ((targets == 0) && (damage != My<Terran_Dropship>::damage_t::none))
					{
						for (const auto & hisUnit : him().AllUnits())
							if (airAttack(hisUnit.second.Type(), him().Player()) || groundAttack(hisUnit.second.Type(), him().Player()))
								if (roundedDist(hisUnit.second.LastPosition(), u->Pos()) < 15*32)
									return false;

						for (const auto & b : him().Buildings())
							if (distToRectangle(u->Pos(), b->TopLeft(), b->Size()) < 10*32)
								return false;

						return true;
					}
				}
	return false;
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Dropping1T1V
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Dropping1T1V *> Dropping1T1V::m_Instances;

Dropping1T1V::Dropping1T1V(My<Terran_Dropship> * pAgent)
	: Behavior(pAgent, behavior_t::Dropping1T1V)
{
	assert_throw(Agent()->Is(Terran_Dropship));

	m_Units = ThisDropship()->LoadedUnits();
	
	m_repairOnly = pAgent->GetDamage() == My<Terran_Dropship>::damage_t::require_repair;

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

	m_inStateSince = ai()->Frame();

///	ai()->SetDelay(100);
}


Dropping1T1V::~Dropping1T1V()
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


void Dropping1T1V::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string Dropping1T1V::StateName() const
{CI(this);
	switch(State())
	{
	case landing:		return "landing";
	default:			return "?";
	}
}


void Dropping1T1V::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);

	if (MyUnit * u = other->IsMyUnit())
		really_remove(m_Units, u);
}



My<Terran_Dropship>::damage_t Dropping1T1V::GetDamage() const
{
	return dropshipDamage(ThisDropship(), m_Units);
}


My<Terran_Dropship> * Dropping1T1V::ThisDropship() const
{CI(this);
	return Agent()->IsMy<Terran_Dropship>();
}


My<Terran_Siege_Tank_Tank_Mode> * Dropping1T1V::GetTank() const
{
	for (BWAPIUnit * u : m_Units)
		if (My<Terran_Siege_Tank_Tank_Mode> * pTank = dynamic_cast<My<Terran_Siege_Tank_Tank_Mode> *>(u))
			return pTank;
			
	return nullptr;
}


vector<My<Terran_SCV> *> Dropping1T1V::GetSCVs() const
{
	vector<My<Terran_SCV> *> List;
	
	for (BWAPIUnit * u : m_Units)
		if (My<Terran_SCV> * pSCV = u->IsMyUnit()->IsMy<Terran_SCV>())
			List.push_back(pSCV);

	return List;
}


bool Dropping1T1V::SafePlace() const
{CI(this);
	return Targets(Agent()) == 0;
}




bool Dropping1T1V::DropshipBeingRepaired() const
{CI(this);
	for (Dropping * pDropping : Dropping::Instances())
		if (pDropping->RepairedUnit() == Agent())
			return true;

	return false;
}


void Dropping1T1V::OnFrame_v()
{CI(this);
///	if (m_repairOnly) for (int i = 0 ; i < 3 ; ++i) bw->drawCircleMap(Agent()->Pos(), 64 + i, Colors::Yellow);

	if (!Agent()->CanAcceptCommand()) return;

	if (m_Units.size() < 5) return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

	switch (State())
	{
	case landing:		OnFrame_landing(); break;
	default: assert_throw(false);
	}
}


void Dropping1T1V::OnFrame_landing()
{CI(this);
	if (JustArrivedInState())
	{
//		assert_throw(Agent()->Loaded());
	///	bw << StateName() << " " << ai()->Frame() << " " << ai()->Frame() - m_inStateSince << endl;
		m_inStateSince = ai()->Frame();
	}

	if (m_repairOnly)
		if (Targets(Agent()) >= 1)
		{
		///	bw << Agent()->NameWithId() << "repairOnly : threat -> GO" << endl;
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		}

	if (Targets(Agent()) >= 3)
	{
	///	bw << Agent()->NameWithId() << "unsafe -> GO" << endl;
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}

	if (Targets(Agent()) == 0)
		if (GetDamage() == My<Terran_Dropship>::damage_t::none)
		{
		///	bw << Agent()->NameWithId() << "safe and repaired -> GO" << endl;
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		}


	if (!ThisDropship()->LoadedUnits().empty())
		if (ThisDropship()->CanUnload())
		{
			if (ai()->Frame() - ThisDropship()->LastUnload() >= 10)
			{
				if (ThisDropship()->GetTank()) return ThisDropship()->Unload(ThisDropship()->GetTank());
				if (ThisDropship()->GetVulture()) return ThisDropship()->Unload(ThisDropship()->GetVulture());
				if (!ThisDropship()->GetSCVs().empty()) return ThisDropship()->Unload(ThisDropship()->GetSCVs().front());
			}
/*
				return ThisDropship()->UnloadAll();
				for (MyUnit * u : m_Units)
					if (u->Loaded())
						if (u->Life() > u->MaxLife()/3)
						{
							return ThisDropship()->Unload(u);
						}*/
		}

	if (!ThisDropship()->LoadedUnits().empty())
		if (!ThisDropship()->CanUnload())
		{
		///	bw << Agent()->NameWithId() << "cannot unload units -> GO" << endl;
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		}


	if (My<Terran_Siege_Tank_Tank_Mode> * pTank = GetTank())
		if (!pTank->Loaded())
			if (!DropshipBeingRepaired())
			{
				Vect dir = toVect(pTank->Pos() - Agent()->Pos());
				if (dir.Norm() < 1) dir = Vect(1.0, 1.0);
				dir.Normalize();
				dir *= 32*8;
				Position delta = toPosition(dir);

				Position target = ai()->GetMap().Crop(Agent()->Pos() + delta);
			///	drawLineMap(Agent()->Pos(), target, Colors::White);
				m_lastMove = ai()->Frame();
				Agent()->Move(target);
			}
			else
				Agent()->Move(pTank->Pos());

}



} // namespace iron



