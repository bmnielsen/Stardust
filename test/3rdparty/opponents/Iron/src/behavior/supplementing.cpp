//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "supplementing.h"
#include "fleeing.h"
#include "repairing.h"
#include "defaultBehavior.h"
#include "../units/cc.h"
#include "../units/army.h"
#include "../territory/stronghold.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Supplementing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Supplementing *> Supplementing::m_Instances;


Supplementing::Supplementing(My<Terran_SCV> * pSCV)
	: Behavior(pSCV, behavior_t::Supplementing)
{
	assert_throw(pSCV->GetStronghold() && pSCV->GetStronghold()->HasBase());

	m_pBase = pSCV->GetStronghold()->HasBase();

	pSCV->GetStronghold()->HasBase()->AddSupplementer(this);

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);
}


Supplementing::~Supplementing()
{
#if !DEV
	try //3
#endif
	{
		assert_throw(contains(m_Instances, this));
		really_remove(m_Instances, this);

		assert_throw(Agent()->GetStronghold() && Agent()->GetStronghold()->HasBase());
		Agent()->GetStronghold()->HasBase()->RemoveSupplementer(this);
	}
#if !DEV
	catch(...){} //3
#endif
}


bool Supplementing::CanRepair(const MyBWAPIUnit * pTarget, int distance) const
{CI(this);
	return ((pTarget->GetStronghold() == Agent()->GetStronghold()) || (distance < 32*32));
}


bool Supplementing::CanChase(const HisUnit * pTarget) const
{CI(this);
	return (pTarget->GetArea(check_t::no_check) == m_pBase->BWEMPart()->GetArea());
}


void Supplementing::OnFrame_v()
{CI(this);
	if (!Agent()->CanAcceptCommand()) return;

	if (Agent()->Life() < 10)
		if (!findThreats(Agent(), 4*32).empty())
			return Agent()->ChangeBehavior<Fleeing>(Agent());

	if (Agent()->Life() < Agent()->PrevLife(30)) return Agent()->ChangeBehavior<Fleeing>(Agent());

	if (Agent()->Life() < Agent()->MaxLife())
		if (Agent()->RepairersCount() < Agent()->MaxRepairers())
			if (Repairing * pRepairer = Repairing::GetRepairer(Agent()))
				return Agent()->ChangeBehavior<Repairing>(Agent(), pRepairer);

	if (!findThreats(Agent(), 3*32).empty())
		return Agent()->ChangeBehavior<Fleeing>(Agent());

	if (me().Army().KeepTanksAtHome())
		if (Wall * pWall = me().StartingVBase()->GetWall())
			return Agent()->Move(ai()->GetVMap().RandomPosition(pWall->Center(), 6*32));

	return Agent()->Move(GetBase()->BWEMPart()->Center());
}






} // namespace iron



