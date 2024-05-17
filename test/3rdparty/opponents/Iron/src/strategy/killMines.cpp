//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "killMines.h"
#include "../units/factory.h"
#include "../units/cc.h"
#include "../behavior/sieging.h"
#include "../behavior/killingMine.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{




//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class KillMines
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


KillMines::KillMines()
{
}


KillMines::~KillMines()
{
}


string KillMines::StateDescription() const
{
	if (!m_active) return "-";
	if (m_active) return "active";

	return "-";
}


int timeToAttack(const MyUnit * pMyUnit, const HisUnit * pTarget)
{
	assert_throw(!pTarget->Flying());

	if (pMyUnit->Is(Terran_Siege_Tank_Siege_Mode))
	{
//		if (const My<Terran_Siege_Tank_Tank_Mode> * pTank = dynamic_cast<const My<Terran_Siege_Tank_Tank_Mode> *>(pMyUnit))
//			if (pTank->GetBehavior()->IsSieging() && (pTank->GetBehavior()->IsSieging()->State() == Sieging::sieging))
//				if (pTank->CanSiegeAttack(pTarget))
//					return pMyUnit->CoolDown();

		return 1000000;
	}

	if (pMyUnit->GetBehavior()->IsSieging()) return 1000000;


	int airDist = roundedDist(pMyUnit->Pos(), pTarget->Pos());
	if (airDist > 10*32) return 1000000;
	if ((airDist > 3*32) && pMyUnit->Is(Terran_SCV)) return 1000000;

	int distToRange = 0;
	if (airDist > pMyUnit->GroundRange())
		distToRange = (pMyUnit->Flying() ? airDist : groundDist(pMyUnit->Pos(), pTarget->Pos())) - pMyUnit->GroundRange();

	frame_t timeToRange = distToRange / 4;
//	bw << pMyUnit->NameWithId() << " is at " << timeToRange << " , " << pMyUnit->CoolDown() << endl;
	return max(timeToRange, pMyUnit->CoolDown());
}


void KillMines::Process(HisUnit * pHisMine, bool dangerous)
{
	assert_throw(ai()->GetMap().Valid(pHisMine->Pos()));
#if DEV
	for (int i = 0 ; i < 5 ; ++i)
		bw->drawCircleMap(pHisMine->Pos(), 20 + i, Colors::White);
#endif

	if (!dangerous)
	{
	///	ai()->SetDelay(1000);
		return;
	}

	if (any_of(KillingMine::Instances().begin(), KillingMine::Instances().end(),
			[pHisMine](const KillingMine * pMineKiller) { return pMineKiller->Target() == pHisMine; }))
		return;

	multimap<int, MyUnit *> Candidates;
	for (MyUnit * u : me().Units())
		if (u->Completed() && !u->Loaded())
			if (u->GetBehavior()->IsChecking() ||
				u->GetBehavior()->IsExploring() ||
				u->GetBehavior()->IsFighting() ||
				u->GetBehavior()->IsGuarding() ||
				u->GetBehavior()->IsKiting() ||
				u->GetBehavior()->IsRaiding() ||
				u->GetBehavior()->IsRazing() ||
				u->GetBehavior()->IsRepairing() ||
				u->GetBehavior()->IsVChasing() ||
				u->GetBehavior()->IsChasing() ||
				u->GetBehavior()->IsConstructing() ||
				u->GetBehavior()->IsFleeing() ||
				u->GetBehavior()->IsHarassing() ||
				u->GetBehavior()->IsPatrollingBases() ||
				u->GetBehavior()->IsRetraiting())
			{
				frame_t t = timeToAttack(u, pHisMine);
				if (t < 40)
					Candidates.emplace(t, u);
			}

	auto iCandEnd = Candidates.begin();
	int remainingDamageToDo = pHisMine->Life();
	for ( ; (iCandEnd != Candidates.end()) && (remainingDamageToDo > 0) ; ++iCandEnd)
		remainingDamageToDo -= FaceOff(iCandEnd->second, pHisMine).MyAttack();

	if (remainingDamageToDo > 0) return;
	
///	ai()->SetDelay(1000);
///	bw << "KILL " << pHisMine->NameWithId() << ": " << endl;
	for (auto iCand = Candidates.begin() ; iCand != iCandEnd ; ++iCand)
	{
///		bw << "  --> " << iCand->second->NameWithId() << endl;
		iCand->second->ChangeBehavior<KillingMine>(iCand->second, pHisMine);
	}
///	bw << endl;
}


void KillMines::OnFrame_v()
{
	if (!him().IsTerran()) return;

	vector<HisUnit *> BuryingMines;
	vector<HisUnit *> DangerousMines;

	for (const unique_ptr<HisUnit> & u : him().Units())
		if (u->Is(Terran_Vulture_Spider_Mine))
			if (!u->InFog())
				if (u->Burrowed() ||								// u is unburying so it is dangerous
					contains(m_OldDangerousMines, u.get()))		// u still dangerous once unburrowed
					DangerousMines.push_back(u.get());
				else
					BuryingMines.push_back(u.get());

	m_OldDangerousMines = DangerousMines;

	if (!m_active)
	{
		if (!(DangerousMines.empty() && BuryingMines.empty()))
		{
			m_active = true;
			m_activeSince = ai()->Frame();
			
		//	if (!DangerousMines.empty()) ai()->SetDelay(2000);
		}
	}


	if (m_active)
	{
		if (DangerousMines.empty() && BuryingMines.empty())
		{
			m_active = false;
			return;
		}

		for (HisUnit * u : DangerousMines)
			Process(u, bool("dangerous"));

		for (HisUnit * u : BuryingMines)
			Process(u, !bool("dangerous"));
	}
}




} // namespace iron



