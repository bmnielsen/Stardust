//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "harassing.h"
#include "fleeing.h"
#include "walking.h"
#include "chasing.h"
#include "exploring.h"
#include "repairing.h"
#include "repairing.h"
#include "defaultBehavior.h"
#include "../strategy/strategy.h"
#include "../strategy/cannonRush.h"
#include "../strategy/walling.h"
#include "../units/cc.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

const TilePosition dimCC = UnitType(Terran_Command_Center).tileSize();



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Harassing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Harassing *> Harassing::m_Instances;

Harassing::Harassing(My<Terran_SCV> * pAgent)
	: Behavior(pAgent, behavior_t::Harassing), m_pHisBase(him().StartingVBase())
{
	assert_throw(m_pHisBase);
	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

	if (Agent()->GetStronghold()) Agent()->LeaveStronghold();
}


Harassing::~Harassing()
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


void Harassing::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
	m_pCurrentTarget = nullptr;
}


string Harassing::StateName() const
{CI(this);
	switch(State())
	{
	case reachingHisBase:	return "reachingHisBase";
	case retreating:		return "retreating";
	case attackNearest:		return "attackNearest";
	default:				return "?";
	}
}


void Harassing::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);
	if (m_pCurrentTarget == other) 
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
}


bool Harassing::CanRepair(const MyBWAPIUnit * pTarget, int ) const
{CI(this);
	if (ai()->GetStrategy()->Active<Walling>())
		return false;

	if (State() == attackNearest)
		if (m_lastAttack || (Agent()->Life() < m_lifeBeforeAttack)) return false;

	if (State() == retreating)
		return false;

	if (Agent()->DistToHisStartingBase() < 10*32)
		if (pTarget->DistToHisStartingBase() / max(1, Agent()->DistToHisStartingBase()) > 3)
			return false;

	if (Agent()->DistToHisStartingBase() < 7*32)
		if ((Agent()->Life() >= 55) && (pTarget->Life() >= 40))
			return false;

	return true;
}


bool Harassing::CanChase(const HisUnit * ) const
{CI(this);
	if (State() == reachingHisBase) return true;
	if (!m_lastAttack && (Agent()->Life() == m_lifeBeforeAttack)) return true;
	return false;
}


const MyUnit * Harassing::FindFrontSoldier() const
{CI(this);
	const MyUnit * pNearest = nullptr;

	for (UnitType type : {Terran_Vulture})
		for (const auto & u : me().Units(type))
			if (u->Completed() && !u->Loaded())
				if (!pNearest || (u->DistToHisStartingBase() < pNearest->DistToHisStartingBase()))
					pNearest = u.get();

	return pNearest;
}


void Harassing::OnFrame_v()
{CI(this);
#if DEV
	if (m_pCurrentTarget) drawLineMap(Agent()->Pos(), m_pCurrentTarget->Pos(), GetColor());//1
#endif


	if (!Agent()->CanAcceptCommand()) return;

	switch (State())
	{
	case reachingHisBase:	OnFrame_reachingHisBase(); break;
	case retreating:		OnFrame_retreating(); break;
	case attackNearest:		OnFrame_attackNearest(); break;
	default: assert_throw(false);
	}
}


void Harassing::OnFrame_reachingHisBase()
{CI(this);
	const bool inHisArea = (Agent()->GetArea() == HisBase()->BWEMPart()->GetArea());

	if (Agent()->Life() < Agent()->PrevLife(10))
		return Agent()->ChangeBehavior<Fleeing>(Agent());

	auto Threats5 = findThreats(Agent(), 4*32);

	for (auto * faceOff : Threats5)
		if (!faceOff->His()->InFog())
			if (faceOff->His()->Unit()->isConstructing())
				if (Agent()->Life() > 15)
				{
				//	{bw << "Ahhh !  " << Agent()->NameWithId() << endl; ai()->SetDelay(5000);}
					return Agent()->ChangeBehavior<Chasing>(Agent(), faceOff->His(), bool("insist"));
				}

	if ((ai()->Frame() - Agent()->LastDangerReport() < 5) || (Agent()->Life() < 30))
		if (!Threats5.empty())
			return Agent()->ChangeBehavior<Fleeing>(Agent());


	auto Threats3 = findThreats(Agent(), 3*32, nullptr);
	if (!Threats3.empty())
	{
	//	if (!inHisArea) return Agent()->ChangeBehavior<Fleeing>(Agent());

		for (auto * faceOff : Threats3)
			if (!faceOff->His()->Type().isWorker())
				return Agent()->ChangeBehavior<Fleeing>(Agent());

		if (Agent()->DistToHisStartingBase() < 32*7)
			if (Threats3.size() <= 2)
				if (Agent()->Life() >= 30)
				{
					ResetSubBehavior();
					return ChangeState(attackNearest);
				}
	}


	if (Agent()->Life() < Agent()->MaxLife())
		if (Agent()->RepairersCount() < Agent()->MaxRepairers())
			if (Repairing * pRepairer = Repairing::GetRepairer(Agent(), (Agent()->Life() > 50) ? 10*32 : (Agent()->Life() > 40) ? 20*32 : 1000000))
				return Agent()->ChangeBehavior<Repairing>(Agent(), pRepairer);

	if (Agent()->DistToHisStartingBase() < 32*4)
		if (Agent()->Life() >= 30)
		{
			ResetSubBehavior();
			return ChangeState(attackNearest);
		}

	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
		SetSubBehavior<Walking>(Agent(), him().StartingBase()->Center(), __FILE__ + to_string(__LINE__));
	}

	if (!inHisArea)
		if (!him().StartingBaseDestroyed())
			if (!ai()->GetStrategy()->Detected<CannonRush>())
				if (!contains(me().EnlargedAreas(), Agent()->GetArea()))
					if (!me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) && !me().CompletedUnits(Terran_Goliath))
						if (!him().MayDarkTemplar())
							if (const MyUnit * pFrontSoldier = Harassing::FindFrontSoldier())
							{
								m_setbackDist = 20*32;
								if (him().IsZerg()) m_setbackDist = 2*32;

								if (Agent()->DistToHisStartingBase() - m_setbackDist < pFrontSoldier->DistToHisStartingBase())
								{
									ResetSubBehavior();
									return ChangeState(retreating);
								}
							}

	if (ai()->Frame() - m_inStateSince > 5)
	{
		if (ai()->Frame() - Agent()->LastFrameMoving() > 50)
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());


		if (!Agent()->SoldierForever())
			if (ai()->Frame() % 200 == 0)
				return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}
}


void Harassing::OnFrame_retreating()
{CI(this);
	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
		SetSubBehavior<Walking>(Agent(), me().StartingBase()->Center(), __FILE__ + to_string(__LINE__));
	///	bw << Agent()->NameWithId() << " is retreating" << endl;
	///	ai()->SetDelay(500);
		return;
	}

	if (Agent()->Life() < Agent()->PrevLife(10))
		return Agent()->ChangeBehavior<Fleeing>(Agent());

	auto Threats3 = findThreats(Agent(), 3*32, nullptr);
	if (!Threats3.empty())
		return Agent()->ChangeBehavior<Fleeing>(Agent());


	if (const MyUnit * pFrontAttacker = Harassing::FindFrontSoldier())
		if (Agent()->DistToHisStartingBase() - (m_setbackDist + 2*32) > pFrontAttacker->DistToHisStartingBase())
		{
			ResetSubBehavior();
			return ChangeState(reachingHisBase);
		}

	if (ai()->Frame() - m_inStateSince > 5)
	{
		if (ai()->Frame() - Agent()->LastFrameMoving() > 50)
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}
}


void Harassing::OnFrame_attackNearest()
{CI(this);
	if (JustArrivedInState())
	{
		m_lifeBeforeAttack = Agent()->Life();
		m_timesCouldChaseRatherThanAttackNearest = 0;
//		ai()->SetDelay(500);
	}

	if (me().SCVsoldiers() < 3)
		return Agent()->ChangeBehavior<Exploring>(Agent(), Agent()->GetArea());

	if (Agent()->Life() < Agent()->PrevLife(10))
		return Agent()->ChangeBehavior<Fleeing>(Agent());

	if (Agent()->CoolDown() >= Agent()->AvgCoolDown()-3)
	{
		m_lastAttack = ai()->Frame();
	//	return Agent()->ChangeBehavior<Fleeing>(Agent());
	}


	if (ai()->Frame() - m_lastAttack > Agent()->AvgCoolDown() + 20)
		if (ai()->Frame() - Agent()->LastDangerReport() < 5)
			if (!findThreats(Agent(), 4*32).empty())
				return Agent()->ChangeBehavior<Fleeing>(Agent());

	auto Threats3 = findThreats(Agent(), 3*32);
	if (!Threats3.empty())
	{
		for (auto * faceOff : Threats3)
			if (!faceOff->His()->Type().isWorker())
				return Agent()->ChangeBehavior<Fleeing>(Agent());
	}

	if ((Agent()->Life() < Agent()->MaxLife()) && (Agent()->Life() == m_lifeBeforeAttack))
		if (Agent()->RepairersCount() < Agent()->MaxRepairers())
			if (Repairing * pRepairer = Repairing::GetRepairer(Agent(), (Agent()->Life() > 50) ? 10*32 : (Agent()->Life() > 40) ? 20*32 : 1000000))
				return Agent()->ChangeBehavior<Repairing>(Agent(), pRepairer);


	map<int, HisUnit *> Candidates;
	for (const auto & faceOff : Agent()->FaceOffs())
		if (HisUnit * pHisUnit = faceOff.His()->IsHisUnit())
			if (!pHisUnit->InFog())
				if (pHisUnit->Type().isWorker())
				{
					if (ai()->Frame() - pHisUnit->PursuingTargetLastFrame() < 30) continue;
					if (pHisUnit->GetArea() != HisBase()->BWEMPart()->GetArea()) continue;

					int & prevDist = m_prevDistances[pHisUnit];
//					int dist = faceOff.GroundDistance();
					int dist = faceOff.GroundDistanceToHitHim();
					int malus = dist;

				//	malus += pHisUnit->LifeWithShields()*2;

					if (toVect(pHisUnit->Pos() - Agent()->Pos()) * Agent()->Acceleration() < 0)
						malus *= 2;

					if (pHisUnit->Unit()->isMoving())
					{
						if (prevDist && (dist >= prevDist) && (dist > 40)) malus += 100000;
					}
					else malus /= 2;

					Candidates[malus] = pHisUnit;
					prevDist = dist;
				}

	m_pCurrentTarget = Candidates.empty() ? nullptr : Candidates.begin()->second;
	if (m_pCurrentTarget)
	{
		if (Agent()->Pos().getApproxDistance(m_pCurrentTarget->Pos()) < 64)
		{
			auto Threats5 = findThreats(Agent(), 5*32);
			if (Threats5.size() == 1)
				if (Threats5.front()->His() == m_pCurrentTarget)
					if (Threats5.front()->FramesToKillMe() > Threats5.front()->FramesToKillHim() + 10)
						if (++m_timesCouldChaseRatherThanAttackNearest == 1)
						{
						///	bw << "hey 0!" << endl; ai()->SetDelay(2000);
							auto pCurrentTarget = m_pCurrentTarget;
							return Agent()->ChangeBehavior<Chasing>(Agent(), pCurrentTarget);
						}
		}

		Agent()->Attack(m_pCurrentTarget);
	}
	else
//		OnFrame_noEnemyWorker();
		Agent()->ChangeBehavior<Exploring>(Agent(), Agent()->GetArea());
}


void Harassing::OnFrame_noEnemyWorker()
{CI(this);
/*
	if (ai()->Frame() % 64 == 0)
	{
		if (rand() % 16 != 0)
			if (HisBuilding * pTarget = findBuildingToRaze(Agent()))
				return Agent()->Attack(pTarget, check_t::no_check);

		if (rand() % 4 != 0)
			return Agent()->Move(ai()->GetVMap().RandomPosition(	him().StartingBase()->Center(),
																((rand() % 4 == 0) ? 12 : 6)*32));
		else
			return Agent()->Move(ai()->GetVMap().RandomPosition(	him().StartingBase()->Center(),
																((rand() % 4 == 0) ? 64 : 32)*32));
	}
*/
}


} // namespace iron



