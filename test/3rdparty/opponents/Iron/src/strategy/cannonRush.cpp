//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "cannonRush.h"
#include "../units/him.h"
#include "../units/my.h"
#include "../behavior/mining.h"
#include "../behavior/scouting.h"
#include "../behavior/chasing.h"
#include "../behavior/defaultBehavior.h"
#include "../behavior/walking.h"
#include "../strategy/strategy.h"
#include "../units/cc.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class CannonRush
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


CannonRush::CannonRush()
{
}


CannonRush::~CannonRush()
{
	for (auto & e : m_map_HisBuilding_MyDefenders)
	{
		const vector<My<Terran_SCV> *> & AssignedDefenders = e.second;
		for (My<Terran_SCV> * pSCV : AssignedDefenders)
			pSCV->ChangeBehavior<DefaultBehavior>(pSCV);
	}

	ai()->GetStrategy()->SetMinScoutingSCVs(1);
}


string CannonRush::StateDescription() const
{
	if (!m_detected) return "-";
	if (m_detected) return "detected";

	return "-";
}


void CannonRush::OnBWAPIUnitDestroyed(BWAPIUnit * pBWAPIUnit)
{
	if (pBWAPIUnit->IsMyUnit())
		if (My<Terran_SCV> * pSCV = pBWAPIUnit->IsMyUnit()->IsMy<Terran_SCV>())
			for (auto & e : m_map_HisBuilding_MyDefenders)
			{
				vector<My<Terran_SCV> *> & AssignedDefenders = e.second;
				really_remove(AssignedDefenders, pSCV);
			}

	// The case of pBWAPIUnit beeing some HisBuilding used as a key in m_map_HisBuilding_MyDefenders is handled in OnFrame_v()
}


vector<Position> CannonRush::HotPositions() const
{
	vector<Position> List;
	for (const auto & e : m_map_HisBuilding_MyDefenders)
	{
		HisBuilding * pHisBuilding = him().FindBuilding(e.first);
		assert_throw(pHisBuilding);

		if (pHisBuilding->Is(Protoss_Pylon) || pHisBuilding->Is(Protoss_Photon_Cannon))
			List.push_back(pHisBuilding->Pos());
	}

	return List;
}


int CannonRush::DistanceToHotPositions(Position from) const
{
	int minDist = numeric_limits<int>::max();
	for (Position p : HotPositions())
		minDist = min(minDist, roundedDist(p, from));

	return minDist;
}


// pylons, no cannons -> 1 defender per incompleted pylon, 4 per completed pylon
// pylons, cannons -> 2 to 5 defenders per cannon based on RemainingBuildTime
// no pylons, cannons -> 1 defender per cannon
int CannonRush::WantedDefenders (const HisBuilding * pHisBuilding, int countPylons, int countCannons) const
{
	if (m_noMoreDefenders) return 0;

	if (pHisBuilding->Is(Protoss_Photon_Cannon))
		if (countPylons) return 3;//max(0, 5 - 5*pHisBuilding->RemainingBuildTime() / pHisBuilding->Type().buildTime());
		else			 return 1;
	else if (pHisBuilding->Is(Protoss_Pylon))
		if (countCannons) return max(0, 50 - pHisBuilding->LifeWithShields()) / 10;
		else              return pHisBuilding->Completed() ? 4 : 1;
	else return 0;
}


void CannonRush::OnFrame_v()
{
	if (him().IsTerran() || him().IsZerg()) return Discard();
	if (me().Bases().size() >= 2) return Discard();
	
	const vector<const Area *> MyEnlargedAreas = me().EnlargedAreas();
	vector<const Area *> MyTerritory = MyEnlargedAreas;
	for (const Area * area : MyEnlargedAreas)
		for (const Area * neighbour : area->AccessibleNeighbours())
			push_back_if_not_found(MyTerritory, neighbour);


	int countPylons = 0;
	int countCannons = 0;
	int countGateways = 0;
	vector<HisBuilding *> IntrusiveBuildings;
	for (const unique_ptr<HisBuilding> & b : him().Buildings())
		if (contains(MyTerritory, b->GetArea(check_t::no_check)) && groundDist(b->Pos(), me().StartingBase()->Center()) < 40*32)
		{
			if      (b->Is(Protoss_Pylon))         ++countPylons;
			else if (b->Is(Protoss_Photon_Cannon)) ++countCannons;
			else if (b->Is(Protoss_Gateway))       ++countGateways;
			else continue;

			IntrusiveBuildings.push_back(b.get());
		}

	vector<HisUnit *> IntrusiveUnits;
	for (const unique_ptr<HisUnit> & u : him().Units())
		if (contains(MyTerritory, u->GetArea(check_t::no_check)) && groundDist(u->Pos(), me().StartingBase()->Center()) < 40*32)
			IntrusiveUnits.push_back(u.get());

	bool oldDetected = m_detected;
	m_detected = countPylons + countCannons > 0;

	if (m_detected || oldDetected)
	{
	///	DO_ONCE { ai()->SetDelay(500); bw << Name() << " started!" << endl; }


		if (me().HasResearched(TechTypes::Tank_Siege_Mode) && (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) >= 3)) return Discard();

		for (HisUnit * u : IntrusiveUnits)
			if (u->Is(Protoss_Probe))
				if (u->Chasers().size() < 2)
					if (My<Terran_SCV> * pWorker = findFreeWorker_urgent(me().StartingVBase()))
						return pWorker->ChangeBehavior<Chasing>(pWorker, u, !bool("insist"), 500);

		vector<My<Terran_SCV> *> ExceedingDefenders;

		// m_map_HisBuilding_MyDefenders-IntrusiveBuildings consistency (part 1)
		// release all the defenders assigned to buildings that are no more in IntrusiveBuildings (i.e. that have been destroyed)
		for (auto it = m_map_HisBuilding_MyDefenders.begin() ; it != m_map_HisBuilding_MyDefenders.end() ; )
		{
			if (none_of(IntrusiveBuildings.begin(), IntrusiveBuildings.end(), [&it](const HisBuilding * b){ return b->Unit() == it->first; }))
			{
				for (My<Terran_SCV> * pSCV : it->second)
					ExceedingDefenders.push_back(pSCV);

				m_map_HisBuilding_MyDefenders.erase(it++);
			}
			else ++it;
		}

		// m_map_HisBuilding_MyDefenders-IntrusiveBuildings consistency (part 2)
		for (HisBuilding * b : IntrusiveBuildings)
			if (!contains(m_HisBuildingsProtected, b->Unit()))
				if (m_map_HisBuilding_MyDefenders.find(b->Unit()) == m_map_HisBuilding_MyDefenders.end())
					m_map_HisBuilding_MyDefenders.emplace(b->Unit(), vector<My<Terran_SCV> *>());


		// Proceeds assignments (part 1: release exceeding defenders)
		for (auto & e : m_map_HisBuilding_MyDefenders)
		{
			HisBuilding * pHisBuilding = him().FindBuilding(e.first);
			assert_throw(pHisBuilding);
			vector<My<Terran_SCV> *> & AssignedDefenders = e.second;

			int wanted = WantedDefenders(pHisBuilding, countPylons, countCannons);
			while ((int)AssignedDefenders.size() > wanted)
			{
				ExceedingDefenders.push_back(AssignedDefenders.back());
				AssignedDefenders.pop_back();
			}
		}

		// Proceeds assignments (part 2)
		for (auto & e : m_map_HisBuilding_MyDefenders)
		{
			HisBuilding * pHisBuilding = him().FindBuilding(e.first);
			assert_throw(pHisBuilding);

			if (contains(m_HisBuildingsProtected, pHisBuilding->Unit())) continue;

			vector<My<Terran_SCV> *> & AssignedDefenders = e.second;

			int wanted = WantedDefenders(pHisBuilding, countPylons, countCannons);
			while ((int)AssignedDefenders.size() < wanted)
			{
				if (!ExceedingDefenders.empty())
				{
					AssignedDefenders.push_back(ExceedingDefenders.back());
					ExceedingDefenders.pop_back();
				}
				else if (My<Terran_SCV> * pWorker = findFreeWorker_urgent(me().StartingVBase()))
				{
					AssignedDefenders.push_back(pWorker);
					pWorker->ChangeBehavior<Walking>(pWorker, pHisBuilding->Pos(), __FILE__ + to_string(__LINE__));
				}

				else
					break;
			}
		}

		// select DefaultBehavior for all the remaining defenders:
		for (My<Terran_SCV> * pSCV : ExceedingDefenders)
			pSCV->ChangeBehavior<DefaultBehavior>(pSCV);

		// handles defenders
		for (auto & e : m_map_HisBuilding_MyDefenders)
		{
			HisBuilding * pHisBuilding = him().FindBuilding(e.first);
			assert_throw(pHisBuilding);
			vector<My<Terran_SCV> *> & AssignedDefenders = e.second;

			// When a defender is hit by a cannon:
			if (any_of(AssignedDefenders.begin(), AssignedDefenders.end(),
						[](const My<Terran_SCV> * pSCV) { return pSCV->Life() < pSCV->PrevLife(10) - 15; }))
			{
				while (!AssignedDefenders.empty())
				{
					AssignedDefenders.back()->ChangeBehavior<DefaultBehavior>(AssignedDefenders.back());
					AssignedDefenders.pop_back();
				}

				if (!contains(m_HisBuildingsProtected, pHisBuilding->Unit()))
					m_HisBuildingsProtected.push_back(pHisBuilding->Unit());

				m_noMoreDefenders = true;
			}

			for (My<Terran_SCV> * pSCV : AssignedDefenders)
			{
				if (const Walking * pWalking = pSCV->GetBehavior()->IsWalking())
				{
					if (pWalking->Target() != pHisBuilding->Pos())
						pSCV->ChangeBehavior<Walking>(pSCV, pHisBuilding->Pos(), __FILE__ + to_string(__LINE__));
					else
						if (pWalking->State() == Walking::succeeded)
							if (pSCV->CanAcceptCommand())
								pSCV->Attack(pHisBuilding);
				}
				else assert_throw_plus(false, pSCV->GetBehavior()->Name());
			}
		}

/*
		DO_ONCE
		{
			for (const auto & b : me().Buildings())
				if (b->CanAcceptCommand())
					if (!b->Completed())
						if (!b->Is(Terran_Refinery))
							if (contains(me().EnlargedAreas(), b->GetArea()))
							{
								int d = DistanceToHotPositions(b->Pos())/32;
								if (d < 20) 
								{
									d -= b->RemainingBuildTime()/(b->Is(Terran_Barracks) ? 200 : 100);
									if (d < 12)
										b->CancelConstruction();
								}
							}
			return;
		}
*/

		DO_ONCE
		{
			if (me().Buildings(Terran_Factory).size() == 2)
			{
				MyBuilding * pLatestUncompletedFactory = nullptr;
				for (const auto & b : me().Buildings(Terran_Factory))
					if (!b->Completed())
						if (!pLatestUncompletedFactory || (b->RemainingBuildTime() > pLatestUncompletedFactory->RemainingBuildTime()))
							pLatestUncompletedFactory = b.get();

				if (pLatestUncompletedFactory && pLatestUncompletedFactory->CanAcceptCommand())
					return pLatestUncompletedFactory->CancelConstruction();
			}
		}

		// Quickly counterattack the enemy base with a bunch of Scouting SCVs.
		DO_ONCE ai()->GetStrategy()->SetMinScoutingSCVs(5);

		// Since there may be Mining SCVs lacking from time to time during the cannon rush,
		// we don't want the new Scouting SCVs to go back to home.
		for (Scouting * pScout : Scouting::Instances())
			if (auto * pSCV = pScout->Agent()->IsMy<Terran_SCV>())
				pSCV->SetSoldierForever();
	}
	else
	{
		if (me().HasResearched(TechTypes::Tank_Siege_Mode) && (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) >= 1)) return Discard();
	}
}


} // namespace iron



