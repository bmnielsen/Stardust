//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "healing.h"
#include "kiting.h"
#include "defaultBehavior.h"
#include "../territory/vgridMap.h"
#include "../behavior/checking.h"
#include "../units/barracks.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

MyUnit * Healing::FindTarget(const My<Terran_Medic> * pMedic)
{
	if (!pMedic->Completed()) return nullptr;
	if (pMedic->Loaded()) return nullptr;
	if (pMedic->Energy() == 0) return nullptr;

	MyUnit * pTarget = nullptr;
	int minDist = 10*32;
	for (UnitType type : {Terran_Marine, Terran_Medic, Terran_Firebat, Terran_Ghost, Terran_SCV})
	for (auto & u : me().Units(type))
		if (u.get() != pMedic)
			if (!u->GetHealer())
				if (u->Completed() && !u->Loaded())
					if (u->Life() <= u->MaxLife() - 10)
						if ((u->Life() + 2*pMedic->Energy() - 1)/10 > (u->Life() - 1)/10)
							{
								int d = roundedDist(u->Pos(), pMedic->Pos());
								if (d < minDist)
								{
									minDist = d;
									pTarget = u.get();
								}
							}

	if (pTarget)
		if (him().IsProtoss())
			for (const FaceOff & fo : pMedic->FaceOffs())
				if (fo.His()->Is(Protoss_Reaver) || fo.His()->Is(Protoss_Scarab))
					if (fo.DistanceToHisRange() < 5*32)
						return nullptr;

	return pTarget;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Healing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Healing *> Healing::m_Instances;


Healing::Healing(My<Terran_Medic> * pAgent)
	: Behavior(pAgent, behavior_t::Healing)
{
	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

	m_inStateSince = ai()->Frame();
///	ai()->SetDelay(100);
}


Healing::~Healing()
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


void Healing::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();

}


string Healing::StateName() const
{CI(this);
	switch(State())
	{
	case healing:			return "healing";
	default:				return "?";
	}
}


void Healing::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);
	if (m_pTarget == other)
		m_pTarget = nullptr;
}


void Healing::OnFrame_v()
{CI(this);
#if DEV
	if (m_pTarget)
	{
		drawLineMap(Agent()->Pos(), m_pTarget->Pos(), GetColor());
	}
#endif

	if (!Agent()->CanAcceptCommand()) return;

	if (Agent()->Life() < Agent()->PrevLife(10))
		return Agent()->ChangeBehavior<Kiting>(Agent());
	switch (State())
	{
	case healing:		OnFrame_healing(); break;
	default: assert_throw(false);
	}
}


void Healing::OnFrame_healing()
{CI(this);
	if (JustArrivedInState())
	{
		m_pTarget = nullptr;
		m_inStateSince = ai()->Frame();
	}

	if (!m_pTarget)
	{
		m_pTarget = Healing::FindTarget(Agent());
		if (!m_pTarget) return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		return Agent()->Heal(m_pTarget);
	}

	if (Agent()->Unit()->isIdle())
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

	if (m_pTarget->Life() > m_pTarget->MaxLife() - 10)
	{
	//	bw << Agent()->NameWithId() << " stops healing " << m_pTarget->NameWithId() << " at " << m_pTarget->Life() << endl;
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}

	if (ai()->Frame() - m_inStateSince > 50)
		if (!m_pTarget->Unit()->isBeingHealed())
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
}



} // namespace iron



