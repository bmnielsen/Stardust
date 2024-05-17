//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "raiding.h"
#include "sieging.h"
#include "dropping1T.h"
#include "dropping1T1V.h"
#include "kiting.h"
#include "healing.h"
#include "walking.h"
#include "repairing.h"
#include "exploring.h"
#include "defaultBehavior.h"
#include "../units/factory.h"
#include "../units/starport.h"
#include "../units/barracks.h"
#include "../units/army.h"
#include "../strategy/strategy.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/shallowTwo.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Raiding
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Raiding *> Raiding::m_Instances;

Raiding::Raiding(MyUnit * pAgent, Position target)
	: Behavior(pAgent, behavior_t::Raiding), m_target(target)
{
	assert_throw(!pAgent->Is(Terran_SCV));
	assert_throw(pAgent->Flying() || pathExists(pAgent->Pos(), target));


	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

/*
	bool keepMarinesAtHome = false;
	if (const ShallowTwo * pShallowTwo = ai()->GetStrategy()->Has<ShallowTwo>())
		if (pShallowTwo->KeepMarinesAtHome())
			keepMarinesAtHome = true;
*/

	if (Agent()->GetStronghold())
		if (!pAgent->Is(Terran_Marine))// || him().IsProtoss() && !keepMarinesAtHome)
		if (!(pAgent->Is(Terran_Goliath) && me().Army().KeepGoliathsAtHome()))
		if (!(pAgent->Is(Terran_Siege_Tank_Tank_Mode) && me().Army().KeepTanksAtHome()))
		if (!(pAgent->Is(Terran_Medic) && (me().Army().KeepGoliathsAtHome() || me().Army().KeepTanksAtHome())))
			Agent()->LeaveStronghold();
}


Raiding::~Raiding()
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


void Raiding::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


bool Raiding::CanRepair(const MyBWAPIUnit * , int ) const
{CI(this);
	return false;
}


bool Raiding::CanChase(const HisUnit * ) const
{CI(this);
	return true;
}


bool Raiding::WaitGroup() const
{CI(this);
	if (!Agent()->Flying())
		if (ai()->GetMap().GetMiniTile(WalkPosition(Agent()->Pos())).Altitude() > 6*32)
		{
			int vultures = 0;
			int others = 0;
			int distToTarget = groundDist(Agent()->Pos(), Target());
			int maxDistToTargetInRaidersAround = numeric_limits<int>::min();
			for (const Raiding * r : Instances())
				if (!r->Agent()->Flying())
					if (!(Agent()->Is(Terran_Vulture) && r->Agent()->Is(Terran_Marine)))
						if (r != this)
							if (r->Target() == Target())
								if (groundDist(r->Agent()->Pos(), Agent()->Pos()) < 20*32)
								{
									int d = groundDist(r->Agent()->Pos(), Target());
									maxDistToTargetInRaidersAround = max(maxDistToTargetInRaidersAround, d);

									if (r->Agent()->Is(Terran_Vulture))
										++vultures;
									else ++others;
							}
	
			const int maxVultures = others ? 2 : 8;

			const int maxSpaceInGroup = (Agent()->Is(Terran_Marine) || Agent()->Is(Terran_Medic))
				? (4 + others/5) * 32
				: 7*32;
			if (distToTarget + maxSpaceInGroup < maxDistToTargetInRaidersAround)
				if (vultures <= maxVultures)
				if (vultures <= 8)
				if (others <= 30)
				{
#if DEV
					for (const Raiding * r : Instances())
						if (!r->Agent()->Flying())
							drawLineMap(Agent()->Pos(), r->Agent()->Pos(), GetColor());
#endif
				///	bw << Agent()->NameWithId() << " wait group" << endl;
				///	ai()->SetDelay(500);
					return true;
				}
		}

	return false;
}


void Raiding::OnFrame_v()
{CI(this);
#if DEV
	drawLineMap(Agent()->Pos(), m_target, GetColor());//1
#endif
	if (!Agent()->CanAcceptCommand()) return;

	m_waiting = false;

	if (Agent()->Life() < Agent()->PrevLife(10))
		return Agent()->ChangeBehavior<Kiting>(Agent());

/*
	if (const ShallowTwo * pShallowTwo = ai()->GetStrategy()->Has<ShallowTwo>())
		if (pShallowTwo->KeepMarinesAtHome())
			if (!Agent()->Flying() && !Agent()->Is(Terran_Vulture))
				return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
*/

	if (Sieging::EnterCondition(Agent()))
		return Agent()->ChangeBehavior<Sieging>(Agent()->As<Terran_Siege_Tank_Tank_Mode>());
	
	if (Agent()->IsMy<Terran_Dropship>())
		if (Dropping1T1V::EnterCondition(Agent()))
			if ((Agent()->IsMy<Terran_Dropship>()->GetDamage() != My<Terran_Dropship>::damage_t::require_repair) ||
				(ai()->Frame() - m_inStateSince >= 40))
				return Agent()->ChangeBehavior<Dropping1T1V>(Agent()->As<Terran_Dropship>());

	if (Kiting::KiteCondition(Agent()) || Kiting::AttackCondition(Agent()))
		return Agent()->ChangeBehavior<Kiting>(Agent());

	if (My<Terran_Medic> * pMedic = Agent()->IsMy<Terran_Medic>())
		if (Healing::FindTarget(pMedic))
			return Agent()->ChangeBehavior<Healing>(pMedic);


	if (Agent()->Life() < Agent()->MaxLife())
		if (Agent()->Type().isMechanical())
			if (Agent()->RepairersCount() < Agent()->MaxRepairers())
				if (Repairing * pRepairer = Repairing::GetRepairer(Agent(),
					Agent()->StayInArea() ? 8*32 :
					!Agent()->Flying() && (Agent()->GetArea(check_t::no_check) == him().GetArea()) ? 8*32 :
					(Agent()->Life()*4 > Agent()->MaxLife()*3) ? 10*32 :
					Agent()->Flying() && Agent()->IsMyUnit() ? 1000000 :
					(Agent()->Life()*4 > Agent()->MaxLife()*2) ? 30*32 : 1000000))
				{
					return Agent()->ChangeBehavior<Repairing>(Agent(), pRepairer);
				}


	if (dist(Agent()->Pos(), Target()) < 32*5)
		if (Agent()->GetArea(check_t::no_check))
		{
			if (!Agent()->Flying()) ResetSubBehavior();
	//		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
			return Agent()->ChangeBehavior<Exploring>(Agent(), Agent()->GetArea(check_t::no_check));
		}

	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
		if (Agent()->Flying())
			return Agent()->Move(Target());
		else
			return SetSubBehavior<Walking>(Agent(), Target(), __FILE__ + to_string(__LINE__));
	}

	if (const Healing * pHealer = Agent()->GetHealer())
		if (pHealer->Agent()->Energy()*2 >= Agent()->MaxLife() - Agent()->Life())
		{
			m_waiting = true;
			return ResetSubBehavior();
		}

	if (WaitGroup())
	{
		m_waiting = true;
		return ResetSubBehavior();
	}

	if (ai()->Frame() - m_inStateSince > 5)
	{
		if (!Agent()->Flying())
			if (!GetSubBehavior())
				if (ai()->Frame() - Agent()->LastFrameMoving() > 10)
					return SetSubBehavior<Walking>(Agent(), Target(), __FILE__ + to_string(__LINE__));

		if (ai()->Frame() - Agent()->LastFrameMoving() > 20)
		{
			if (Agent()->GetArea(check_t::no_check))
				return Agent()->ChangeBehavior<Exploring>(Agent(), Agent()->GetArea(check_t::no_check));

			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		}

//		if (ai()->Frame() % 100 == 0)
//			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}
}



} // namespace iron



