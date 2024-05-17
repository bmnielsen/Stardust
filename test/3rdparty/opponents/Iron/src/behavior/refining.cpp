//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "refining.h"
#include "fleeing.h"
#include "repairing.h"
#include "exploring.h"
#include "defaultBehavior.h"
#include "../units/cc.h"
#include "../territory/stronghold.h"
#include "../strategy/strategy.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/cannonRush.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Refining
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Refining *> Refining::m_Instances;


Refining::Refining(My<Terran_SCV> * pSCV)
	: Behavior(pSCV, behavior_t::Refining)
{
	assert_throw(pSCV->GetStronghold() && pSCV->GetStronghold()->HasBase());

	m_pBase = pSCV->GetStronghold()->HasBase();

	assert_throw(!m_pBase->BWEMPart()->Geysers().empty());
	assert_throw(m_pBase->BWEMPart()->Geysers().front()->Ext());

	pSCV->GetStronghold()->HasBase()->AddRefiner(this);

	Geyser * g = m_pBase->BWEMPart()->Geysers().front();
	Assign(g);

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);
}


Refining::~Refining()
{
#if !DEV
	try //3
#endif
	{
		assert_throw(contains(m_Instances, this));
		really_remove(m_Instances, this);

		assert_throw(Agent()->GetStronghold() && Agent()->GetStronghold()->HasBase());
		Agent()->GetStronghold()->HasBase()->RemoveRefiner(this);

		Release();
	}
#if !DEV
	catch(...){} //3
#endif
}


void Refining::Assign(Geyser * g)
{CI(this);
	assert_throw(g && !m_pGeyser);
	assert_throw(g->Data() < 3);		// Cf. class Refining comments
	m_pGeyser = g;
	g->SetData(g->Data() + 1);
}


void Refining::Release()
{CI(this);
	assert_throw(m_pGeyser);
	assert_throw(m_pGeyser->Data() >= 1);		// Cf. class Refining comments
	m_pGeyser->SetData(m_pGeyser->Data() - 1);
	m_pGeyser = nullptr;
}


bool Refining::MovingTowardsRefinery() const
{CI(this);
	return (!Agent()->Unit()->isCarryingGas()) && Agent()->Unit()->isMoving();
}


void Refining::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);
	if (other->TopLeft() == m_pGeyser->TopLeft()) 
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
}


bool Refining::CanRepair(const MyBWAPIUnit * pTarget, int distance) const
{CI(this);
	return MovingTowardsRefinery()
		&& ((pTarget->GetStronghold() == Agent()->GetStronghold()) || (distance < 32*32));
}


bool Refining::CanChase(const HisUnit * pTarget) const
{CI(this);
	return pTarget->GetArea(check_t::no_check) == m_pBase->BWEMPart()->GetArea();
}


void Refining::OnFrame_v()
{CI(this);
#if DEV
	drawLineMap(Agent()->Pos(), GetGeyser()->Pos(), GetColor());//1
#endif

	if (!Agent()->CanAcceptCommand()) return;

	if (JustArrivedInState())
	{
		return Agent()->Gather(GetGeyser());
	}

	if (Agent()->Life() < 10)
		if (!findThreats(Agent(), 4*32).empty())
			return Agent()->ChangeBehavior<Fleeing>(Agent());

	int dangerousUnits = 0;
	int dangerousFlyingUnits = 0;
	bool dangerousBuildings = false;
	auto Threats3 = findThreats(Agent(), 3*32);
	for (auto * faceOff : Threats3)
	{
		if (faceOff->His()->IsHisBuilding()) dangerousBuildings = true;
		else
		{
			if (!faceOff->His()->Type().isWorker()) ++dangerousUnits;
			if (faceOff->His()->Flying()) ++dangerousFlyingUnits;
		}
	}

	if (!ai()->GetStrategy()->Detected<CannonRush>())
		if (Agent()->Life() < Agent()->PrevLife(30))
			if (/*!ai()->GetStrategy()->Detected<EnemyScout>() ||*/ dangerousUnits || dangerousBuildings)
			{
	//			if (ai()->GetStrategy()->Detected<CannonRush>() && dangerousBuildings && !dangerousUnits)
	//				return Agent()->ChangeBehavior<Exploring>(Agent(), Agent()->GetArea());

				if (dangerousBuildings || (dangerousUnits > dangerousFlyingUnits) || (dangerousFlyingUnits > 4))
					return Agent()->ChangeBehavior<Fleeing>(Agent());
			}

	if (Agent()->Life() < Agent()->MaxLife())
		if (Agent()->RepairersCount() < Agent()->MaxRepairers())
			if (Repairing * pRepairer = Repairing::GetRepairer(Agent()))
				return Agent()->ChangeBehavior<Repairing>(Agent(), pRepairer);

/*
	if (Agent()->Unit()->isMoving())
		if (!ai()->GetStrategy()->Detected<EnemyScout>() || dangerousUnits || dangerousBuildings)
		if (!ai()->GetStrategy()->Detected<CannonRush>() || dangerousUnits || !dangerousBuildings)
			if (!Threats3.empty())
				if (dangerousBuildings || (dangerousUnits > dangerousFlyingUnits) || (dangerousFlyingUnits > 4))
					return Agent()->ChangeBehavior<Fleeing>(Agent());
*/

	if (Agent()->Unit()->isIdle())
		return Agent()->Gather(GetGeyser());
}







} // namespace iron



