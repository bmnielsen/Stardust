//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "mining.h"
#include "fleeing.h"
#include "blocking.h"
#include "repairing.h"
#include "exploring.h"
#include "walking.h"
#include "defaultBehavior.h"
#include "../territory/stronghold.h"
#include "../strategy/strategy.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/zealotRush.h"
#include "../strategy/cannonRush.h"
#include "../units/cc.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Mining
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Mining *> Mining::m_Instances;


Mining::Mining(My<Terran_SCV> * pSCV)
	: Behavior(pSCV, behavior_t::Mining)
{
	assert_throw(pSCV->GetStronghold() && pSCV->GetStronghold()->HasBase());

	m_pBase = pSCV->GetStronghold()->HasBase();

	pSCV->GetStronghold()->HasBase()->AddMiner(this);

	Mineral * m = FindFreeMineral();
	assert_throw(m);
	Assign(m);

	m_state = (Agent()->Unit()->isCarryingMinerals() || Agent()->Unit()->isCarryingGas()) ? returning : gathering;
	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);
	m_inStateSince = ai()->Frame();
	m_miningSince = ai()->Frame();
}


Mining::~Mining()
{
#if !DEV
	try //3
#endif
	{
		assert_throw(contains(m_Instances, this));
		really_remove(m_Instances, this);

		assert_throw(Agent()->GetStronghold() && Agent()->GetStronghold()->HasBase());
		Agent()->GetStronghold()->HasBase()->RemoveMiner(this);

		Release();
	}
#if !DEV
	catch(...){} //3
#endif
}


// Finds a free Mineral to gather.
// Minerals not assigned yet are prefered first, then the closest ones (to the Command Center).
Mineral * Mining::FindFreeMineral() const
{CI(this);
	map<int, Mineral *> MineralsByDist;
	for (Mineral * m : GetBase()->BWEMPart()->Minerals())
		if (m->Data() < 2)	// Cf. class Mineral comments
		{
			int threatFactor = 0;
			{
				int minDistToThreats = numeric_limits<int>::max();
				for (const FaceOff * fo : findThreatsButWorkers(Agent(), 3*32))
					threatFactor = minDistToThreats = min(minDistToThreats, roundedDist(fo->His()->Pos(), m->Pos()));

				for (const unique_ptr<HisBuilding> & b : him().Buildings())
					if (b->GroundThreatBuilding() && b->Completed())
						if (b->GetArea(check_t::no_check) == m_pBase->BWEMPart()->GetArea())
							if (roundedDist(b->Pos(), m->Pos()) < b->GroundRange() + 5*32)
								threatFactor = minDistToThreats = min(minDistToThreats, roundedDist(b->Pos(), m->Pos()));
			}

			const int distToCC = distToRectangle(GetBase()->BWEMPart()->Center(), m->TopLeft(), m->Size());

			MineralsByDist[-10000*threatFactor + 1000*m->Data() + distToCC] = m;
		}

	return MineralsByDist.empty() ? nullptr : MineralsByDist.begin()->second;
}


void Mining::Assign(Mineral * m)
{CI(this);
	assert_throw(m && !m_pMineral);
	assert_throw(m->Data() < 2);		// Cf. class Mining comments
	m_pMineral = m;
	m->SetData(m->Data() + 1);
}


void Mining::Release()
{CI(this);
	assert_throw(m_pMineral);
	assert_throw(m_pMineral->Data() >= 1);		// Cf. class Mining comments
	m_pMineral->SetData(m_pMineral->Data() - 1);
	m_pMineral = nullptr;
}


bool Mining::MovingTowardsMineral() const
{CI(this);
	return (m_state == gathering) && Agent()->Unit()->isMoving();
}


void Mining::OnMineralDestroyed(Mineral *)
{CI(this);
	Agent()->ChangeBehavior<DefaultBehavior>(Agent());
}


bool Mining::CanRepair(const MyBWAPIUnit * pTarget, int distance) const
{CI(this);

	if (pTarget->Is(Terran_Missile_Turret) && pTarget->GetStronghold())
		if (distance < 40*32)
			return true;


	if (him().ZerglingPressure() || him().HydraPressure())
		if (pTarget->Is(Terran_Bunker) && pTarget->GetStronghold())
			if (distance < 40*32)
				return true;

	if (pTarget->IsMyBuilding())
		if (distance < 10*32)
			return true;

	return (MovingTowardsMineral() || ai()->GetStrategy()->Detected<ZerglingRush>())
		&& ((me().MineralsAvailable() > 100) || (Mining::Instances().size() >= 3))
		&& ((pTarget->GetStronghold() == Agent()->GetStronghold()) || (distance < 32*32));
}


bool Mining::CanChase(const HisUnit * pTarget) const
{CI(this);
	return ((me().MineralsAvailable() > 100) || (Mining::Instances().size() >= 3))
		&& (pTarget->GetArea() == m_pBase->BWEMPart()->GetArea());
}


void Mining::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	m_state = st;
	OnStateChanged();
	m_inStateSince = ai()->Frame();
}


void Mining::OnFrame_v()
{CI(this);
#if DEV
	drawLineMap(Agent()->Pos(), GetMineral()->Pos(), GetColor());//1
#endif
//	bw << Agent()->NameWithId() << " ... " << Agent()->m_lastCommandExecutionFrame << endl;
	if (!Agent()->CanAcceptCommand()) return;

	if (Agent()->Life() < 10)
		if (!findThreats(Agent(), 4*32).empty())
			return Agent()->ChangeBehavior<Fleeing>(Agent());

/*
	if (ai()->GetStrategy()->Detected<ZerglingRush>())
		if (ai()->Frame() - m_miningSince > 400)
		if (!findThreats(Agent(), 3*32).empty())
		{
			bw->drawCircleMap(Agent()->Pos(), 10, Colors::Green, true);
			return Agent()->ChangeBehavior<Mining>(Agent());
		}
*/

	switch (State())
	{
	case gathering:		OnFrame_gathering(); break;
	case returning:		OnFrame_returning(); break;
	default: assert_throw(false);
	}
}



void Mining::OnFrame_gathering()
{CI(this);
	if (Agent()->Unit()->isCarryingMinerals()) return ChangeState(returning);

	// TODO: make a state for this if-else
	if (GetSubBehavior() && GetSubBehavior()->IsWalking())
	{
		if (GetSubBehavior()->IsWalking()->State() == Walking::succeeded)
		{
			m_inStateSince = ai()->Frame();
			ResetSubBehavior();
		}

		return;
	}
	else
	{
		if (ai()->Frame() - m_inStateSince > 500)
		{
		///	bw << Agent()->NameWithId() << " was stuck" << endl;
			return SetSubBehavior<Walking>(Agent(), Agent()->GetStronghold()->HasBase()->BWEMPart()->Center(), __FILE__ + to_string(__LINE__));
		}
	}

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

	if (Agent()->Life() < Agent()->PrevLife(30))
		if (/*!ai()->GetStrategy()->Detected<EnemyScout>() ||*/ dangerousUnits || dangerousBuildings)
		{
			if (ai()->GetStrategy()->Detected<CannonRush>() && dangerousBuildings && !dangerousUnits)
				return Agent()->ChangeBehavior<Exploring>(Agent(), Agent()->GetArea());

			if (dangerousBuildings || (dangerousUnits > dangerousFlyingUnits) || (dangerousFlyingUnits > 4))
				return Agent()->ChangeBehavior<Fleeing>(Agent());
		}

	if (Agent()->Life() < Agent()->MaxLife())
		if (Blocking::Instances().empty())
			if ((Agent()->Life() <= 15)
				|| (me().MineralsAvailable() > 80)
				|| (Mining::Instances().size() > 7))
				if (Agent()->RepairersCount() < Agent()->MaxRepairers())
					if (Repairing * pRepairer = Repairing::GetRepairer(Agent()))
						return Agent()->ChangeBehavior<Repairing>(Agent(), pRepairer);
/*
	if (!ai()->GetStrategy()->Detected<ZerglingRush>())
	if (!ai()->GetStrategy()->Detected<ZealotRush>())
	if (!ai()->GetStrategy()->Detected<EnemyScout>() || dangerousUnits || dangerousBuildings)
	if (!ai()->GetStrategy()->Detected<CannonRush>() || dangerousUnits || !dangerousBuildings)
		if (!Threats3.empty())
			if (dangerousBuildings || (dangerousUnits > dangerousFlyingUnits) || (dangerousFlyingUnits > 4))
				return Agent()->ChangeBehavior<Fleeing>(Agent());
*/

	if (Agent()->Unit()->isIdle() ||
		(	(distToRectangle(Agent()->Pos(), GetMineral()->TopLeft(), GetMineral()->Size()) > 18) &&
			(Agent()->PrevPos(0) != Agent()->PrevPos(1)) &&
			Agent()->MovingAwayFrom(GetMineral()->Pos())
		))
			
	{
		if (!GetMineral()->Unit()->isVisible()) return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		return Agent()->Gather(GetMineral());
	}
}


void Mining::OnFrame_returning()
{CI(this);
	if (!Agent()->Unit()->isCarryingMinerals()) return ChangeState(gathering);

//	if (!(Agent()->Unit()->isCarryingMinerals() ||
//		  Agent()->Unit()->isCarryingGas()))		// so that gas gathered previously is not lost
//		return ChangeState(gathering);

	if (JustArrivedInState() || Agent()->Unit()->isIdle())
		return Agent()->ReturnCargo();

}





} // namespace iron



