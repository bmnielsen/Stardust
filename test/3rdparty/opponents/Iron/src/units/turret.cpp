//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "turret.h"
#include "../territory/stronghold.h"
#include "../behavior/repairing.h"
#include "../strategy/strategy.h"
#include "../strategy/freeTurrets.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

MyBuilding * findAloneTurret(VBase * base, int radius)
{
	MyBuilding * pAloneTurret = nullptr;
	int maxDist = radius;
	for (const auto & b1 : me().Buildings(Terran_Missile_Turret))
		if (b1->GetStronghold() == base->GetStronghold())
			if (!contains(base->FirstTurretsLocations(), b1->TopLeft()))
			{
				int minDist = numeric_limits<int>::max();
				for (const auto & b2 : me().Buildings(Terran_Missile_Turret))
					if (b2->GetStronghold() == base->GetStronghold())
						if (b1.get() != b2.get())
							minDist = min(minDist, roundedDist(b1->Pos(), b2->Pos()));

				if (minDist > maxDist)
				{
					maxDist = minDist;
					pAloneTurret = b1.get();
				}
			}

	return pAloneTurret;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Missile_Turret>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


template<>
class ExpertInConstructing<Terran_Missile_Turret> : public ConstructingExpert
{
public:
						ExpertInConstructing() : ConstructingExpert(Terran_Missile_Turret) {}

	void				UpdateConstructingPriority() override;

private:
};


void ExpertInConstructing<Terran_Missile_Turret>::UpdateConstructingPriority()
{
	m_priority = 0;

	if (me().CompletedBuildings(Terran_Engineering_Bay) == 0) { m_priority = 0; return; }

	if (him().HydraPressure() || him().HydraPressure_needVultures())
		if (!him().MayMuta())
			 { m_priority = 0; return; }

	if (him().IsProtoss())
		if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode)*4 < (int)him().AllUnits(Protoss_Dragoon).size()*3)
			 { m_priority = 0; return; }

	if (him().IsProtoss())
		if (!him().MayCarrier())
		if (!him().MayReaver())
		if (!him().MayDarkTemplar())
		if (!him().MayShuttleOrObserver())
		if (him().AllUnits(Protoss_Scout).empty())
			 { m_priority = 0; return; }

	if (me().FindBuildingNeedingBuilder(Type()))
	{
		m_priority = 299;
		return;
	}

	if (him().IsZerg())
		if (me().Buildings(Terran_Command_Center).size() < 2)
			if (!him().ZerglingPressure())
				if (him().Units(Zerg_Mutalisk).empty() && him().Buildings(Zerg_Spire).empty())
					{ m_priority = 0; return; }


	{
		int maxPriority = 0;
		VBase * pUrgentBase = nullptr;

		for (VBase * base : me().Bases())
		{
			const bool mainBase = base == me().Bases().front();
			int priority = 0;

			int turretsInBase = count_if(me().Buildings(Terran_Missile_Turret).begin(), me().Buildings(Terran_Missile_Turret).end(),
				[base](const unique_ptr<MyBuilding> & b) { return b->GetStronghold() == base->GetStronghold(); });

			//if ((turretsInBase == 0) && (me().Bases().size() >= 2))
			//	priority = 460; // greater than CC's priority
			//else

			if (him().IsProtoss() && (me().Bases().size() == 2) && !mainBase && !base->Active())
				priority = 0;
			else if ((turretsInBase >= 2) &&
				him().IsZerg() &&
				(!him().MayMuta() || (him().HydraPressure_needVultures() && him().Units(Zerg_Mutalisk).empty())))
				priority = 0;
			else
			{
				if (turretsInBase < 5)
				{
					priority = 300 - turretsInBase*40;
					if (!him().AllUnits(Zerg_Mutalisk).empty() ||
						!him().Buildings(Zerg_Spire).empty())
						priority = 460 + 100*((int)him().AllUnits(Zerg_Mutalisk).size() - turretsInBase);

					if (turretsInBase < 4)
						if (!him().AllUnits(Protoss_Dark_Templar).empty())
							priority = 460 + 100*((int)him().AllUnits(Protoss_Dark_Templar).size()/2 - turretsInBase);
				}

				if (auto * pFreeTurrets = ai()->GetStrategy()->Active<FreeTurrets>())
					if (pFreeTurrets->NeedManyTurrets() || pFreeTurrets->NeedManyManyTurrets() || him().IsZerg()/* || him().MayMuta()*/)
					{
						int wantedTurrets = 8;
						if (pFreeTurrets->NeedManyManyTurrets())
						{
							wantedTurrets = 10;
							wantedTurrets += max(0, (int)him().AllUnits(Zerg_Mutalisk).size() - 10)/2;
							wantedTurrets += max(0, (int)him().AllUnits(Zerg_Mutalisk).size() - 20)/3;
							wantedTurrets += max(0, (int)him().AllUnits(Zerg_Mutalisk).size() - 30)/4;
						}
						if (mainBase && (him().AllUnits(Zerg_Mutalisk).size() >= 4)) wantedTurrets = int(wantedTurrets*1.3);

						if (turretsInBase < wantedTurrets)
						{
//							priority = 400 - turretsInBase*(pFreeTurrets->NeedManyManyTurrets() ? 15 : 20);
							priority = (pFreeTurrets->NeedManyManyTurrets() ? 550 : 460) - min(10, turretsInBase)*40;
							if (him().AllUnits(Zerg_Mutalisk).size() >= 4) priority += 10*him().AllUnits(Zerg_Mutalisk).size();
						}

						if (him().IsZerg())
							if (base->CreationTime() == 0)
								if (turretsInBase >= 3 && (turretsInBase <= 6))
									if (MyBuilding * pAloneTurret = findAloneTurret(base, 6*32))
										priority = 550;
					}

			}

			if (priority > maxPriority)
			{
				maxPriority = priority;
				pUrgentBase = base;
			}
		}

		if (pUrgentBase)
		{
			m_priority = maxPriority;
			SetBase(pUrgentBase);
			return;
		}
	}
}


ExpertInConstructing<Terran_Missile_Turret>	My<Terran_Missile_Turret>::m_ConstructingExpert;

ConstructingExpert * My<Terran_Missile_Turret>::GetConstructingExpert() { return &m_ConstructingExpert; }


template<>
class ExpertInConstructingFree<Terran_Missile_Turret> : public ConstructingExpert
{
public:
						ExpertInConstructingFree() : ConstructingExpert(Terran_Missile_Turret) {}

	void				UpdateConstructingPriority() override;

private:
	bool				Free() const override					{ return true; }
	TilePosition		GetFreeLocation() const override;
	My<Terran_SCV> *	GetFreeBuilder() const override;
};


TilePosition ExpertInConstructingFree<Terran_Missile_Turret>::GetFreeLocation() const
{
	if (auto * pFreeTurrets = ai()->GetStrategy()->Active<FreeTurrets>())
		return pFreeTurrets->NextLocation();

	return TilePositions::None;
}


My<Terran_SCV> * ExpertInConstructingFree<Terran_Missile_Turret>::GetFreeBuilder() const
{
	if (auto * pFreeTurrets = ai()->GetStrategy()->Active<FreeTurrets>())
		return pFreeTurrets->Builder();

	return nullptr;
}


void ExpertInConstructingFree<Terran_Missile_Turret>::UpdateConstructingPriority()
{
	m_priority = 0;

	if (me().CompletedBuildings(Terran_Engineering_Bay) == 0) return;

	if (auto * pFreeTurrets = ai()->GetStrategy()->Active<FreeTurrets>())
		if (pFreeTurrets->NextLocation() != TilePositions::None)
			if (pFreeTurrets->Priority() > 0)
			{
				m_priority = pFreeTurrets->Priority();
				return;
			}
}


ExpertInConstructingFree<Terran_Missile_Turret>	My<Terran_Missile_Turret>::m_ConstructingFreeExpert;

ConstructingExpert * My<Terran_Missile_Turret>::GetConstructingFreeExpert() { return &m_ConstructingFreeExpert; }


My<Terran_Missile_Turret>::My(BWAPI::Unit u)
	: MyBuilding(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Missile_Turret);
	assert_throw(m_ConstructingExpert.Unselected() || m_ConstructingFreeExpert.Unselected());

	m_ConstructingExpert.OnBuildingCreated();
	m_ConstructingFreeExpert.OnBuildingCreated();
}


void My<Terran_Missile_Turret>::DefaultBehaviorOnFrame()
{CI(this);
	if (DefaultBehaviorOnFrame_common()) return;

	if (Completed())
	{
		const int distToCC = GetStronghold()
								? roundedDist(Pos(), GetStronghold()->HasBase()->BWEMPart()->Center())
								: 8*32;

		if (ai()->Frame() - m_lastCallForRepairer > 250 ||
			Life() < MaxLife()) m_nextWatchingRadius = max(10*32, distToCC + 8*32);

	///	bw->drawCircleMap(Pos(), 40*32, Colors::White);
	///	bw->drawCircleMap(Pos(), m_nextWatchingRadius, Colors::Green);

		if (RepairersCount() == 0)
			if (any_of(him().Units(Zerg_Mutalisk).begin(), him().Units(Zerg_Mutalisk).end(), [this](const HisUnit * u)
					{ return dist(Pos(), u->Pos()) < m_nextWatchingRadius; }))
			//if (me().CompletedBuildings(Terran_Command_Center) >= 2)
				if (Repairing::GetRepairer(this, 30*32))
				{
				//	ai()->SetDelay(1000);
					m_nextWatchingRadius -= 1*32;	// decrease watching radius each time (avoid exloit)

					m_nextWatchingRadius = max(m_nextWatchingRadius, 7*32);

					m_lastCallForRepairer = ai()->Frame();
					return;
				}
	}
}


int My<Terran_Missile_Turret>::MaxRepairers() const
{CI(this);
	return GetStronghold() ? 4 : 3;
}




	
} // namespace iron



