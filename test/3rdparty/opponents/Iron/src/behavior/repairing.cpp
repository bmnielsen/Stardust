//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "repairing.h"
#include "fleeing.h"
#include "walking.h"
#include "mining.h"
#include "scouting.h"
#include "supplementing.h"
#include "defaultBehavior.h"
#include "../territory/stronghold.h"
#include "../territory/vbase.h"
#include "../strategy/strategy.h"
#include "../strategy/dragoonRush.h"
#include "../strategy/cannonRush.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/marineRush.h"
#include "../strategy/workerRush.h"
#include "../strategy/walling.h"
#include "../units/cc.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Repairing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Repairing *> Repairing::m_Instances;



int Repairing::CountRepairers()
{
	return count_if(Repairing::Instances().begin(), Repairing::Instances().end(),
					[](const Repairing * r){ return r->TargetX(); });
}


Repairing * Repairing::GetRepairer(MyBWAPIUnit * pTarget, int radius)
{
	if (pTarget->Loaded()) return nullptr;
	if (pTarget->IsMyUnit() && !pTarget->IsMyUnit()->WorthBeingRepaired()) return nullptr;

	const int repairers = Repairing::CountRepairers();

	const int baseRepairers = count_if(Repairing::Instances().begin(), Repairing::Instances().end(),
									[](const Repairing * r){ return r->TargetX() && r->Agent()->GetStronghold(); });

	const int miners = (int)Mining::Instances().size();

	const int maxBaseRepairers =
		ai()->GetStrategy()->Detected<DragoonRush>() ? 3 :
		ai()->GetStrategy()->Detected<WorkerRush>() ? 2 :
		1;
	
	if (pTarget->Is(Terran_SCV) && pTarget->GetStronghold() && (baseRepairers >= maxBaseRepairers)) return nullptr;

	if (pTarget->Is(Terran_SCV) && pTarget->GetStronghold())
		if (me().Buildings(Terran_Engineering_Bay).empty())
		{
			const int healthyWorkers = count_if(me().Units(Terran_SCV).begin(), me().Units(Terran_SCV).end(), [](const unique_ptr<MyUnit> & u)
											{ return (u->Life() == u->MaxLife()) &&
													(u->GetBehavior()->IsMining() || u->GetBehavior()->IsRefining() || u->GetBehavior()->IsSupplementing()); });
			if (healthyWorkers >= 3)
			{
				return nullptr;
			}
		}


	if (!(pTarget->IsMyBuilding() && pTarget->IsMyBuilding()->GetWall()))
		if (miners < 2 + ((repairers >= 2) ? 2 : 1)*repairers)
		{
		///	bw << "                  Repairing::GetRepairer : miners = " << miners << " < " << 2 + ((repairers >= 2) ? 2 : 1)*repairers << " -> fail" << endl;
			return nullptr;
		}

	if (Supplementing::Instances().empty())
		if (me().Buildings(Terran_Command_Center).size() == 1)
			if (pTarget->Is(Terran_SCV) && pTarget->GetStronghold())
				if (!ai()->GetStrategy()->Detected<ZerglingRush>())
				if (!ai()->GetStrategy()->Detected<MarineRush>())
				if (!ai()->GetStrategy()->Detected<WorkerRush>())
				if (!ai()->GetStrategy()->Detected<CannonRush>())
					if (count_if(me().StartingVBase()->GetStronghold()->SCVs().begin(), me().StartingVBase()->GetStronghold()->SCVs().end(),
						[](My<Terran_SCV> * pSCV) { return pSCV->Completed() && (pSCV->Life() < pSCV->MaxLife()); }) <= 1)
						return nullptr;

//	const int minDistToHisStartingBase = 32*50;

//	int minDist = min(minDistToTarget, max(minDistToHisStartingBase, pTarget->DistToHisStartingBase()));
	int minDist = radius;
	My<Terran_SCV> * pBestCandidate = nullptr;

	const int lifeToRepair = pTarget->MaxLife() - pTarget->Life();

	for (const auto & u : me().Units(Terran_SCV))
		if (u->Completed() && !u->Loaded())
			if (u.get() != pTarget)
			{
				if (u->GetBehavior()->IsDefaultBehavior()) return nullptr;
//				if (!ai()->GetStrategy()->Active<Walling>())
//					if (u->GetBehavior()->IsDefaultBehavior()) continue;

				int length;
				if (pTarget->Flying())	length = roundedDist(pTarget->Pos(), u->Pos());
				else					ai()->GetMap().GetPath(pTarget->Pos(), u->Pos(), &length);

				if ((length >= 0))// && (length < max(minDistToHisStartingBase, u->DistToHisStartingBase())))
				{
					if (pTarget->Is(Terran_SCV))
						if (u->Life() < u->MaxLife())
							length -= lifeToRepair * (u->MaxLife() - u->Life()) / 2 + 32*5;

					if (pTarget->Is(Terran_Bunker))
						if (ai()->GetStrategy()->Detected<ZerglingRush>() || him().ZerglingPressure() || him().HydraPressure())
							length -= 1000 * u->Life();

					if (length < minDist)
						if (u->GetBehavior()->CanRepair(pTarget, length))
						{
							minDist = length;
							pBestCandidate = u->As<Terran_SCV>();
						}
				}
			}

	if (!pBestCandidate) return nullptr;
	
	pBestCandidate->ChangeBehavior<Repairing>(pBestCandidate, pTarget);
	return pBestCandidate->GetBehavior()->IsRepairing();
}


Repairing::Repairing(MyBWAPIUnit * pAgent, MyBWAPIUnit * pTarget)
	: Behavior(pAgent, behavior_t::Repairing), m_pTarget(pTarget)
{
///	bw << Agent()->NameWithId() << " repairs " << pTarget->NameWithId() << endl;

	assert_throw(Agent()->Is(Terran_SCV));
	assert_throw(pAgent != pTarget);
	assert_throw(!contains(pTarget->Repairers(), this));
	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);
	assert_throw(contains(pTarget->Repairers(), this));
}


Repairing::Repairing(MyBWAPIUnit * pAgent, Repairing * pRepairer)
	: Behavior(pAgent, behavior_t::Repairing), m_pTarget(nullptr)
{
///	bw << Agent()->NameWithId() << " repaired by " << pRepairer->Agent()->NameWithId() << endl;

	assert_throw(this != pRepairer);
	assert_throw(pRepairer->IsRepairer());
	assert_throw(contains(Agent()->Repairers(), pRepairer));
	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);
}


Repairing::~Repairing()
{
#if !DEV
	try //3
#endif
	{
	///	bw << Agent()->NameWithId() << "'s repairing destroyed" << endl;

		assert_throw(!m_beingDestroyed);

		{
			m_beingDestroyed = true;

			MyBWAPIUnit * pTarget = m_pTarget;
			vector<Repairing *> Repairers = Agent()->Repairers();
			for (Repairing * r : Repairers)
				r->m_pTarget = nullptr;

			assert_throw(!pTarget || contains(pTarget->Repairers(), this));
			assert_throw(contains(m_Instances, this));
			really_remove(m_Instances, this);
			assert_throw(!pTarget || !contains(pTarget->Repairers(), this));

			for (Repairing * r : Repairers)
			{
				MyBWAPIUnit * pRepairer = r->Agent();
				if (pRepairer->Repairers().empty())
					pRepairer->ChangeBehavior<DefaultBehavior>(pRepairer);
			}

			if (pTarget)
				if (pTarget->Repairers().empty())
					if (Repairing * r = pTarget->GetBehavior()->IsRepairing())
						if (!r->m_pTarget)
							pTarget->ChangeBehavior<DefaultBehavior>(pTarget);
		}
	}
#if !DEV
	catch(...){} //3
#endif
}


My<Terran_SCV> * Repairing::IsRepairer() const
{CI(this);
	if (m_pTarget)
		if (auto * pSCV = Agent()->IsMy<Terran_SCV>())
				return pSCV;

	return nullptr;
}


void Repairing::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);
	if (other == TargetX())
	{
		if (MyBWAPIUnit * u = other->IsMyBWAPIUnit())
			if (u->GetBehavior())
				if (u->GetBehavior()->IsRepairing())
					return;

		Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}
}


void Repairing::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string Repairing::StateName() const
{CI(this);
	switch(State())
	{
	case reachingTarget:	return "reachingTarget";
	case repairing:			return  IsRepairer() ? "repairing" : "being repaired";
	default:				return "?";
	}
}


void Repairing::OnFrame_v()
{CI(this);
#if DEV
	if (TargetX()) drawLineMap(Agent()->Pos(), TargetX()->Pos(), GetColor());//1
#endif

	if (!Agent()->CanAcceptCommand()) return;

	switch (State())
	{
	case reachingTarget:	OnFrame_reachingTarget(); break;
	case repairing:			OnFrame_repairing(); break;
	default: assert_throw(false);
	}
}


static bool isWorkerNearBase(const MyBWAPIUnit * u)
{
	return u->Is(Terran_SCV) &&
			u->GetStronghold() &&
			(dist(u->GetStronghold()->HasBase()->BWEMPart()->Center(), u->Pos()) < 8*32);
}

void Repairing::OnFrame_reachingTarget()
{CI(this);

	MyBWAPIUnit * pAssociate = TargetX();
	if (!pAssociate)
	{
		assert_throw(!Agent()->Repairers().empty());
		pAssociate = Agent()->Repairers().front()->Agent();
	}

	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();

		if (Agent()->Flying())
			Agent()->Move(pAssociate->Pos());
		else
		{
			int length;
			ai()->GetMap().GetPath(Agent()->Pos(), pAssociate->Pos(), &length);
			if (length >= 0)
				SetSubBehavior<Walking>(Agent(), pAssociate->Pos(), __FILE__ + to_string(__LINE__));
			else
				Agent()->Move(pAssociate->Pos());
		}
	}

	if (ai()->Frame() - m_inStateSince > 50)
		if (Agent()->Unit()->isIdle())
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

//	if (!findPursuers(Agent()).empty())	return Agent()->ChangeBehavior<Fleeing>(Agent());
//	if (Agent()->Life() < Agent()->PrevLife(30)) return Agent()->ChangeBehavior<Fleeing>(Agent()->IsMyUnit());

//	if (ai()->Frame() - Agent()->IsMyUnit()->LastDangerReport() < 5)
//		if (!findThreats(Agent()->IsMyUnit(), 4*32).empty())
//			return Agent()->ChangeBehavior<Fleeing>(Agent()->IsMyUnit());

	const bool skipThreats = TargetX() &&
						(TargetX()->Is(Terran_Siege_Tank_Siege_Mode) ||
						TargetX()->Is(Terran_Bunker) && him().ZerglingPressure() ||
						TargetX()->Is(Terran_Bunker) && ai()->GetStrategy()->Detected<ZerglingRush>() ||
						TargetX()->Is(Terran_Bunker) && ai()->GetStrategy()->Detected<CannonRush>() ||
						TargetX()->Is(Terran_Refinery) && ai()->GetStrategy()->Detected<CannonRush>() ||
						TargetX()->Is(Terran_Bunker) && him().HydraPressure() ||
						TargetX()->IsMyBuilding() && TargetX()->IsMyBuilding()->GetWall()
						);
	const bool skipAirUnits = TargetX() && (TargetX()->Is(Terran_Missile_Turret) || TargetX()->Is(Terran_Goliath));

	if (!skipThreats)
		if (!findThreats(Agent()->IsMyUnit(), 3*32, nullptr, skipAirUnits).empty())
			return Agent()->ChangeBehavior<Fleeing>(Agent()->IsMyUnit());

	int d = roundedDist(Agent()->Pos(), pAssociate->Pos());
	if (pAssociate->IsMyBuilding() && !pAssociate->Flying())
		d = distToRectangle(Agent()->Pos(), pAssociate->TopLeft(), pAssociate->IsMyBuilding()->Size());

	if (d < 32*2)
	{
		ResetSubBehavior();
		return ChangeState(repairing);
	}

	if (d > 32*10)
	{
		// This will check if there is a better repairer:
		const int modulus = //pAssociate->IsMyBuilding() && pAssociate->IsMyBuilding()->PartOfWall() ? 256 :
							pAssociate->Is(Terran_Bunker) ? 256 :
							isWorkerNearBase(Agent()) || isWorkerNearBase(pAssociate) ? 64 :
							32;

		if (ai()->Frame() % modulus == 0)
		{
		///	bw << Agent()->NameWithId() << "check if there is a better repairer" << endl;
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		}
	}
}


void Repairing::OnFrame_repairing()
{CI(this);
	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();

		if (auto * pSCV = IsRepairer())
		{
			if (TargetX()->Life() == TargetX()->MaxLife())
			{
				if (him().IsZerg())
					if (TargetX()->Is(Terran_Barracks))
						if (auto s = ai()->GetStrategy()->Active<Walling>())
							if (!s->GetWall()->Open())
								if (ai()->GetVMap().OutsideWall(ai()->GetMap().GetTile(TilePosition(pSCV->Pos()))))
									return pSCV->ChangeBehavior<Scouting>(pSCV);

				return pSCV->ChangeBehavior<DefaultBehavior>(pSCV);
			}
			return pSCV->Repair(TargetX());
		}
		else
			if (auto * pRepairedSCV = Agent()->IsMy<Terran_SCV>())
				for (Repairing * r : pRepairedSCV->Repairers())
					if (r->Agent()->Life() < r->Agent()->MaxLife())
						return pRepairedSCV->Repair(r->Agent());
	}

	if (ai()->Frame() - m_inStateSince > 50)
		if (Agent()->Unit()->isIdle())
			if (IsRepairer())
				return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

	if (IsRepairer())
		if (TargetX()->Life() == TargetX()->PrevLife(10)) 
		if (TargetX()->Life() == TargetX()->PrevLife(20)) 
		if (TargetX()->Life() == TargetX()->PrevLife(30)) 
		if (TargetX()->Life() == TargetX()->PrevLife(40)) 
		if (TargetX()->Life() == TargetX()->PrevLife(50)) 
			if (ai()->Frame() - m_inStateSince > (isWorkerNearBase(Agent()) ? 80 : 40))
				return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

	const bool skipThreats = TargetX() &&
								(TargetX()->Is(Terran_Siege_Tank_Siege_Mode) ||
								TargetX()->Is(Terran_Bunker) && him().ZerglingPressure() ||
								TargetX()->Is(Terran_Bunker) && ai()->GetStrategy()->Detected<ZerglingRush>() ||
								TargetX()->Is(Terran_Bunker) && ai()->GetStrategy()->Detected<CannonRush>() ||
								TargetX()->Is(Terran_Refinery) && ai()->GetStrategy()->Detected<CannonRush>() ||
								TargetX()->Is(Terran_Bunker) && him().HydraPressure() ||
								TargetX()->IsMyBuilding() && TargetX()->IsMyBuilding()->GetWall());
	const bool skipAirUnits = TargetX() && (TargetX()->Is(Terran_Missile_Turret) || TargetX()->Is(Terran_Goliath));
	bool threats = !skipThreats && !findThreats(Agent()->IsMyUnit(), 4*32, nullptr, skipAirUnits).empty();

//	if (Agent()->Life() < Agent()->PrevLife(30)) return Agent()->ChangeBehavior<Fleeing>(Agent()->IsMyUnit());

//	if (ai()->Frame() - Agent()->IsMyUnit()->LastDangerReport() < 5)
//		if (!findThreats(Agent()->IsMyUnit(), 5*32).empty())
//			return Agent()->ChangeBehavior<Fleeing>(Agent()->IsMyUnit());

//	if (!findThreats(Agent()->IsMyUnit(), 4*32).empty())
//		return Agent()->ChangeBehavior<Fleeing>(Agent()->IsMyUnit());

	if (threats)
		return Agent()->ChangeBehavior<Fleeing>(Agent()->IsMyUnit());


	if (IsRepairer())
		if (double(TargetX()->Life()) / TargetX()->MaxLife() >= TargetX()->MaxLifePercentageToRepair() - 0.000001)
			if (!(TargetX()->GetBehavior()->IsRepairing() && (TargetX()->GetBehavior()->IsRepairing()->TargetX() == Agent())) ||
				(double(Agent()->Life()) / Agent()->MaxLife() >= Agent()->MaxLifePercentageToRepair() - 0.000001))
				return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

	if (!IsRepairer())
		if (double(Agent()->Life()) / Agent()->MaxLife() < Agent()->MaxLifePercentageToRepair()*2/3)
			if (Agent()->RepairersCount() < Agent()->MaxRepairers())
				if (Repairing * pRepairer = Repairing::GetRepairer(Agent(), 16*32))
					return;
}


} // namespace iron



