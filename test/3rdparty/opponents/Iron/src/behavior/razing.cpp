//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "razing.h"
#include "repairing.h"
#include "fleeing.h"
#include "kiting.h"
#include "exploring.h"
#include "defaultBehavior.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Razing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Razing *> Razing::m_Instances;


HisBuilding * Razing::FindBuilding(MyUnit * u)
{//return nullptr;
	if (!u->GetArea(check_t::no_check)) return nullptr;
	if (u->GroundAttack() + u->AirAttack() == 0) return nullptr;

	vector<HisBuilding *> Candidates;
	for (const FaceOff & faceOff : u->FaceOffs())
		if (faceOff.DistanceToMyRange() < 6*32)
			if (HisBuilding * pTarget = faceOff.His()->IsHisBuilding())
				if (!pTarget->InFog())
					if (!pTarget->Flying())
						if (!faceOff.HisAttack())
							Candidates.push_back(pTarget);

	if (Candidates.empty())
		for (auto & b : him().Buildings())
			if (!b->InFog())
				if (!b->Flying())
					if (b->GetArea() == u->GetArea())
						if (!(u->Flying() ? b->AirAttack() : b->GroundAttack()))
							Candidates.push_back(b.get());


	HisBuilding * pBestTarget = nullptr;
	bool mostLife = (u->GetArea() == him().GetArea()) && !u->Is(Terran_SCV);
	int bestTargetLife = mostLife ? numeric_limits<int>::min() : numeric_limits<int>::max();
	for (HisBuilding * b : Candidates)
		if (mostLife == (b->LifeWithShields() > bestTargetLife))
		{
//			assert_throw(b->Unit()->exists());
			bestTargetLife = b->LifeWithShields();
			pBestTarget = b;
		//	if (b->Type().isResourceDepot()) break;
		}

	return pBestTarget;
}


Razing::Razing(MyUnit * pAgent, const Area * pWhere)
	: Behavior(pAgent, behavior_t::Razing), m_pWhere(pWhere)
{
//	assert_throw(pAgent->Flying() || pWhere->AccessibleFrom(pAgent->GetArea()));
	assert_throw(pAgent->GetArea(check_t::no_check) == pWhere);
	assert_throw(Razing::Condition(pAgent));

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

	m_inStateSince = ai()->Frame();

//	static bool done = false; if (!done) { done = true; ai()->SetDelay(500); bw << Agent()->NameWithId() << " starts " << Name() << endl; }
}


Razing::~Razing()
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


void Razing::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


void Razing::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);
	if (m_pTarget == other)
		m_pTarget = nullptr;
}


void Razing::OnFrame_v()
{CI(this);
#if DEV
	if (m_pTarget) drawLineMap(Agent()->Pos(), m_pTarget->Pos(), GetColor());//1
#endif


	if (!Agent()->CanAcceptCommand()) return;

	if (Agent()->Life() < Agent()->PrevLife(10))
		return Agent()->ChangeBehavior<Fleeing>(Agent());

	if (Agent()->GroundRange() > 3*32)
	{
		if (Kiting::KiteCondition(Agent()) || Kiting::AttackCondition(Agent()))
			return Agent()->ChangeBehavior<Kiting>(Agent());
	}
	else
	{
		auto Threats3 = findThreats(Agent(), 3*32);
		if (!Threats3.empty())
			return Agent()->ChangeBehavior<Fleeing>(Agent());
	}

	if (Agent()->Type().isMechanical())
		if (Agent()->Life() < Agent()->MaxLife())
			if (Agent()->RepairersCount() < Agent()->MaxRepairers())
				if (Repairing * pRepairer = Repairing::GetRepairer(Agent(),
							(Agent()->Life()*4 > Agent()->MaxLife()*3) ? 16*32 :
							(Agent()->Life()*4 > Agent()->MaxLife()*2) ? 32*32 :
							(Agent()->Life()*4 > Agent()->MaxLife()*1) ? 64*32 : 1000000))
					return Agent()->ChangeBehavior<Repairing>(Agent(), pRepairer);


	m_pTarget = FindBuilding();
	if (m_pTarget)
	{
		if (m_pLastAttackedTarget != m_pTarget)
			Agent()->Attack(m_pLastAttackedTarget = m_pTarget, check_t::no_check);
	}
	else
		Agent()->ChangeBehavior<Exploring>(Agent(), Where());
}



} // namespace iron



