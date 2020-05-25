//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "barracks.h"
#include "army.h"
#include "../behavior/exploring.h"
#include "../behavior/raiding.h"
#include "../behavior/scouting.h"
#include "../behavior/mining.h"
#include "../behavior/walking.h"
#include "../behavior/healing.h"
#include "../behavior/repairing.h"
#include "../behavior/scouting.h"
#include "../strategy/strategy.h"
#include "../strategy/walling.h"
#include "../strategy/expand.h"
#include "../strategy/workerRush.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/zealotRush.h"
#include "../strategy/wraithRush.h"
#include "../strategy/marineRush.h"
#include "../strategy/cannonRush.h"
#include "../strategy/shallowTwo.h"
#include "../strategy/firstBarracksPlacement.h"
#include "../interactive.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Barracks>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


template<>
class ExpertInTraining<Terran_Marine> : public TrainingExpert
{
public:
						ExpertInTraining(MyBuilding * pWhere) : TrainingExpert(Terran_Marine, pWhere) {}

	void				UpdateTrainingPriority() override;

private:
};

/*
static int getMarinePriorityAgainstProtoss()
{
	if (him().HasCannons()) return 398;

//	if (him().MayDarkTemplar())
//		if ((me().Units(Terran_Marine).size() < 24) && (me().Buildings(Terran_Command_Center).size() >= 2)) return 595;

	if (ai()->GetStrategy()->Has<ShallowTwo>())
		for (const auto & b : me().Buildings(Terran_Factory))
			if (b->Completed())
				if (!b->Unit()->isTraining())
					return 398;
	if (ai()->GetStrategy()->Has<ShallowTwo>() && ai()->GetStrategy()->Has<ShallowTwo>()->DelayExpansion())
		if (me().Units(Terran_Marine).size() < 24) return 615;

	if ((me().Units(Terran_Marine).size() < 4) && (Mining::Instances().size() >= 8)) return 615;
	else if ((me().Units(Terran_Marine).size() < 32) && (Mining::Instances().size() >= 25)) return 615;
//	else if (!him().HasCannons() && (me().Buildings(Terran_Command_Center).size() == 1) && (me().GetVBase(0)->LackingMiners() == 0)) return 615;
	else return 398;
}
*/

void ExpertInTraining<Terran_Marine>::UpdateTrainingPriority()
{/*
	if (him().IsProtoss())
		if (me().GetWall() && me().GetWall()->Active())
			if (me().CreationCount(Terran_Marine) == 0)
				{ m_priority = 100000; return; }
*/
//	if (Units() >= 6) { m_priority = 0; return; }

	if (ai()->GetStrategy()->Active<Walling>())
	{
		if (him().IsZerg())
			if (!me().CompletedBuildings(Terran_Factory))
				if (me().LostUnitsOrBuildings(Terran_Marine) < 3)
					if ((int)him().AllUnits(Zerg_Zergling).size() > Units()*3)
						if (Units() < min(4, Units() + 1))
							{ m_priority = 3000; return; }

		if (Units() < 1)
			if (!him().IsProtoss() || !me().Buildings(Terran_Factory).empty())
				if (me().CreationCount(Terran_Marine) < 2)
					{ m_priority = 3000; return; }
	}

	// deal with gas steal
	if (Units() < 2) 
		for (auto & b : him().Buildings())
			if (b->Type().isRefinery())
				if (b->GetArea() == me().GetArea())
					{ m_priority = 615; return; }

	if (Interactive::moreMarines) { m_priority = 430; return; }

	int maxCount = him().IsTerran() || him().IsProtoss() ? 1 : 2;
	if (him().IsProtoss()) maxCount = 0;
	if (auto s = ai()->GetStrategy()->Detected<WorkerRush>())   maxCount = s->MaxMarines();
	if (auto s = ai()->GetStrategy()->Detected<ZerglingRush>()) maxCount = s->MaxMarines();
	if (auto s = ai()->GetStrategy()->Detected<MarineRush>())   maxCount = s->MaxMarines();
	if (auto s = ai()->GetStrategy()->Detected<WraithRush>()) if (s->ConditionToMakeMarines())   maxCount = Units() + 1;
	if (him().ZerglingPressure() && !me().StartingVBase()->GetWall()) maxCount = 12;
	if (him().HydraPressure()) maxCount = ((me().Units(Terran_Vulture).size() + 2*me().Units(Terran_Siege_Tank_Tank_Mode).size()
											< him().AllUnits(Zerg_Hydralisk).size()/2)
											? 8 : 4);
/*
	if (ai()->GetStrategy()->Detected<CannonRush>())
		if (!me().Buildings(Terran_Bunker).empty())
			if (Units() < 4)
				{ m_priority = 25000; return; }
*/
	if (Units() >= maxCount) { m_priority = 0; return; }

	if (him().ZerglingPressure() && !me().StartingVBase()->GetWall() ||
		him().HydraPressure() ||
		ai()->GetStrategy()->Detected<WorkerRush>() ||
		ai()->GetStrategy()->Detected<ZerglingRush>() ||
		ai()->GetStrategy()->Detected<ZealotRush>() ||
		ai()->GetStrategy()->Detected<MarineRush>() ||
		ai()->GetStrategy()->Detected<WraithRush>())
		if ((Units() < 2) || ((int)Mining::Instances().size() >= (ai()->GetStrategy()->Detected<ZealotRush>() ? 3 : 5)))
		{
			if (ai()->GetStrategy()->Detected<ZerglingRush>() && (Units() >= 5) && (me().CompletedBuildings(Terran_Bunker) >= 1))
				{ m_priority = 595; return; }

			{ m_priority = 615; return; }
		}


/*
	if (him().IsProtoss())
	{
		m_priority = getMarinePriorityAgainstProtoss();
		return;
	}
*/
	if (me().Buildings(Terran_Factory).size() < 1) { m_priority = 0; return; }

	if ((Units() == 1) && (me().Buildings(Terran_Supply_Depot).size() < 3)) { m_priority = 0; return; }

	m_priority = 200;
}


template<>
class ExpertInTraining<Terran_Medic> : public TrainingExpert
{
public:
						ExpertInTraining(MyBuilding * pWhere) : TrainingExpert(Terran_Medic, pWhere) {}

	void				UpdateTrainingPriority() override;

private:
};


/*

0	0		0.5
50	0.5		1
100	1		1.5
150	1.5		2
200	2		2.5
250	2.5		3

*/

void ExpertInTraining<Terran_Medic>::UpdateTrainingPriority()
{
	if (me().GasAvailable() < 10) { m_priority = 0; return; }

	if (him().ZerglingPressure() || him().HydraPressure())
		m_priority = 0;

/*
	if (him().IsProtoss())
	{
		if (int(0.5 + me().MedicForce()*4) < (int)me().Units(Terran_Marine).size())
		{
			m_priority = getMarinePriorityAgainstProtoss() + 2;
			return;
		}
	}
	else
*/
	if (!him().IsProtoss())
	{
		if (Units() < (me().SCVsoldiers() + 5) / 10) 
		{
			m_priority = 420;
			return;
		}
	}
	
	m_priority = 0;
}


template<>
class ExpertInConstructing<Terran_Barracks> : public ConstructingExpert
{
public:
						ExpertInConstructing() : ConstructingExpert(Terran_Barracks) {}

	void				UpdateConstructingPriority() override;

private:
};


void ExpertInConstructing<Terran_Barracks>::UpdateConstructingPriority()
{
	if (Builders() < BuildingsUncompleted()) { m_priority = 590; return; }
	
	if (Buildings() == 0)
	{
		const bool mayBeZerg = !(him().IsProtoss() || him().IsTerran());
		const int start = mayBeZerg ? 10 : 11;
		if (me().SupplyUsed() < start)
			{ m_priority = 0; return; }

		//ert
		if (auto s = ai()->GetStrategy()->Active<Walling>())
			if (him().IsProtoss())
				if ((s->GetWall()->Size() == 1) || (s->GetWall()->Size() == 2))
					if (none_of(me().Units(Terran_SCV).begin(), me().Units(Terran_SCV).end(), [s](const unique_ptr<MyUnit> & u)
							{ return (u->GetBehavior()->IsScouting() || u->GetBehavior()->IsHarassing()) &&
									(groundDist(me().GetBase(0)->Center(), u->Pos()) >
									 groundDist(me().GetBase(0)->Center(), s->GetWall()->Center()) + 6*32); }))
						{ m_priority = 0; return; }

		if (me().SupplyUsed() == start)
			if (me().MineralsAvailable() >= 100)
				if (auto s = ai()->GetStrategy()->Active<Walling>())
				{
					if (s->GetWall()->Size() >= 2)
						if (mayBeZerg)
							for (const auto & b : me().Buildings(Terran_Supply_Depot))
								if (!b->Completed() && (b->RemainingBuildTime() < 300))
								{
								///	DO_ONCE bw << "Terran_Supply_Depot RemainingBuildTime = " << b->RemainingBuildTime() << " - Waiting builder" << endl;
									if (auto s = ai()->GetStrategy()->Has<FirstBarracksPlacement>()) s->Discard();
									m_priority = 0;
									return;
								}

						m_priority = 1001;
						return;
				}

		if (me().SupplyUsed() >= start)	{ m_priority = 610; return; }


		if (ai()->GetStrategy()->Detected<WorkerRush>())
			if (me().MineralsAvailable() < 80) m_priority = 0;

		return;
	}
	
	if (him().ZerglingPressure() && !me().StartingVBase()->GetWall())
	{
		//if (BuildingsUncompleted()) { m_priority = 0; return; }

		if (him().AllUnits(Zerg_Zergling).size() >= 15)
			if (Buildings() <= 1)
			{
				m_priority = 610;
				return;
			}
	}

	if (/*him().IsProtoss() ||*/ Interactive::moreMarines)
	{
		//if (BuildingsUncompleted()) { m_priority = 0; return; }

		if (Buildings() <= 1)
		{
			if (me().SupplyUsed() >= 13)	m_priority = 590;
			else							m_priority = 0;
			return;
		}



		if (me().Buildings(Terran_Command_Center).size() >= 2)
		{
			const int activeBases = count_if(me().Bases().begin(), me().Bases().end(), [](VBase * b){ return b->Active(); });
		//	const int lowerBound = min(12, 2*activeBases);
			const int upperBound = min(12, 3*(int)me().Buildings(Terran_Command_Center).size() - 1);

			if ((int)me().Buildings(Terran_Barracks).size() < upperBound)
			{


	/*
				if (mechProductionSites < lowerBound-1)	{ m_priority = min(500, 390 + elapsed/10); return; }
				if (mechProductionSites < lowerBound)	{ m_priority = min(500, 290 + elapsed/20); return; }
				if (mechProductionSites < upperBound)	{ m_priority = 200 + 10*(upperBound - mechProductionSites); return; }
	*/
				m_priority = 50 * me().BarrackActivity();

			//	if (me().SupplyUsed() >= 195)
				{
					static map<int, frame_t> startingFrameByNumberOfBarracks;
					if (startingFrameByNumberOfBarracks[Buildings()] == 0)
						startingFrameByNumberOfBarracks[Buildings()] = ai()->Frame();
					frame_t elapsed = ai()->Frame() - startingFrameByNumberOfBarracks[Buildings()];

					m_priority = max(m_priority, min(500, 300 + elapsed/30));
				}

				return;
			}
		}
	}

	m_priority = 0;
}


ExpertInConstructing<Terran_Barracks>	My<Terran_Barracks>::m_ConstructingExpert;

ConstructingExpert * My<Terran_Barracks>::GetConstructingExpert() { return &m_ConstructingExpert; }


My<Terran_Barracks>::My(BWAPI::Unit u)
	: MyBuilding(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Barracks);

	AddTrainingExpert<Terran_Marine>();
	AddTrainingExpert<Terran_Medic>();

	m_ConstructingExpert.OnBuildingCreated();
}


void My<Terran_Barracks>::DefaultBehaviorOnFrame()
{CI(this);
	if (DefaultBehaviorOnFrame_common()) return;

	if (GetWall())
		if (Completed())
		{

			if (Flying())
			{
				if (ai()->GetStrategy()->Active<Walling>())
				{
					if (GetWall()->CloseRequest())
					{
						Land(GetWall()->Locations()[IndexInWall()], check_t::no_check);
						return;
					}

					if (ai()->Frame() - LiftedSince() > 120)
					{
						Land(GetWall()->Locations()[IndexInWall()], check_t::no_check);
						return;
					}

				}
			}
			else
			{
				if (!Unit()->isTraining())
				{
/*
					if (!enemyNearby)
						DO_ONCE
							return Lift();
					}
*/

					if (!ai()->GetStrategy()->Active<Walling>())
						return Lift();

					if (ai()->GetStrategy()->Active<Walling>())
						if (GetWall()->OpenRequest())
							return Lift();
				}

				
				if (ai()->GetStrategy()->Active<Walling>())
					if (him().IsZerg())
						if (me().CompletedBuildings(Terran_Barracks))
							if (!him().AllUnits(Zerg_Zergling).empty() || !me().CompletedUnits(Terran_Vulture))
								if (none_of(me().Buildings().begin(), me().Buildings().end(),
									[](const MyBuilding * b){ return b->Completed() && b->Life() < b->MaxLife(); }))
							{
			//					const int distToCC = roundedDist(Pos(), GetStronghold()->HasBase()->BWEMPart()->Center());

			//					if (ai()->Frame() - m_lastCallForRepairer > 250 ||
			//						Life() < MaxLife()) m_nextWatchingRadius = max(10*32, distToCC + 3*32);

							///	bw->drawCircleMap(Pos(), 40*32, Colors::White);
							///	bw->drawCircleMap(Pos(), m_nextWatchingRadius, Colors::Green);

								if (me().LostUnitsOrBuildings(Terran_SCV) >= 4) return;


								if (RepairersCount() < (him().AllUnits(Zerg_Zergling).empty() ? 1 : 2))
									if (Repairing::GetRepairer(this, 50*32))
									{
										//m_lastCallForRepairer = ai()->Frame();
										return;
									}
							}
					
			}
		}

}



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Marine>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

My<Terran_Marine>::My(BWAPI::Unit u)
	: MyUnit(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Marine);
}


void My<Terran_Marine>::DefaultBehaviorOnFrame()
{CI(this);
//	return ChangeBehavior<Walking>(this, Pos(), __FILE__ + to_string(__LINE__));
//	return ChangeBehavior<Walking>(this, ai()->GetMap().Center(), __FILE__ + to_string(__LINE__));

//	if (ai()->GetStrategy()->Detected<ZerglingRush>())
//		if (!GetStronghold())
//			EnterStronghold(me().StartingVBase()->GetStronghold());

	const Area * pNaturalArea = findNatural(me().StartingVBase())->BWEMPart()->GetArea();
/*
	if (him().IsProtoss())
	{
		if (const ShallowTwo * pShallowTwo = ai()->GetStrategy()->Has<ShallowTwo>())
			if (pShallowTwo->KeepMarinesAtHome())
			{
				if (!GetStronghold())
					EnterStronghold(me().StartingVBase()->GetStronghold());

				return ChangeBehavior<Exploring>(this, homeAreaToHold());
			}

		if (him().StartingBase())
		{
			if (him().StartingBase()->GetArea()->AccessibleFrom(GetArea()))
			{
				if (!him().StartingBaseDestroyed())
				{
				///	bw << NameWithId() << " go to his base!" << endl; ai()->SetDelay(2000);
					return ChangeBehavior<Raiding>(this, GetRaidingTarget());
				}
				else
				{
					return ChangeBehavior<Exploring>(this, GetArea());
				}
			}
		}
		else
		{
		///	bw << NameWithId() << " go to his base!" << endl; ai()->SetDelay(2000);
			return ChangeBehavior<Scouting>(this);
		}
	}
	else
*/
	{
		//assert_throw(GetStronghold());

		if (ai()->GetStrategy()->Detected<WorkerRush>() ||
			ai()->GetStrategy()->Detected<ZerglingRush>() ||
			him().ZerglingPressure() ||
			him().HydraPressure() ||
			(me().CompletedBuildings(Terran_Bunker) >= 1) && (me().Bases().size() == 1) ||
			ai()->GetStrategy()->Detected<MarineRush>())
				return ChangeBehavior<Exploring>(this, me().GetArea());

		if (auto s = ai()->GetStrategy()->Active<Walling>())
			return ChangeBehavior<Exploring>(this, homeAreaToHold());

//		if (!him().IsProtoss())
			if (none_of(Exploring::Instances().begin(), Exploring::Instances().end(),
				[](const Exploring * e){ return e->Where() == me().GetArea(); }))
				return ChangeBehavior<Exploring>(this, me().GetArea());

		return ChangeBehavior<Exploring>(this, pNaturalArea);
	}
}



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Medic>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

My<Terran_Medic>::My(BWAPI::Unit u)
	: MyUnit(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Medic);
}


void My<Terran_Medic>::Heal(MyBWAPIUnit * target, check_t checkMode)
{CI(this);
///	bw << NameWithId() + " heal " + target->NameWithId() << endl;
	bool result = Unit()->useTech(TechTypes::Healing, target->Unit());
	OnCommandSent(checkMode, result, NameWithId() + " heal " + target->NameWithId());
}


void My<Terran_Medic>::DefaultBehaviorOnFrame()
{CI(this);
//	return ChangeBehavior<Walking>(this, Pos(), __FILE__ + to_string(__LINE__));
//	return ChangeBehavior<Walking>(this, ai()->GetMap().Center(), __FILE__ + to_string(__LINE__));

	if (me().Army().KeepGoliathsAtHome() || me().Army().KeepTanksAtHome())
	{
		if (!GetStronghold())
			EnterStronghold(me().StartingVBase()->GetStronghold());
		return ChangeBehavior<Exploring>(this, me().Bases().back()->GetArea()->BWEMPart());
	}

	if (Healing::FindTarget(this))
		return ChangeBehavior<Healing>(this);

	const Area * pNaturalArea = findNatural(me().StartingVBase())->BWEMPart()->GetArea();

/*
	if (const ShallowTwo * pShallowTwo = ai()->GetStrategy()->Has<ShallowTwo>())
		if (pShallowTwo->KeepMarinesAtHome())
		{
			if (!GetStronghold())
				EnterStronghold(me().StartingVBase()->GetStronghold());
			return ChangeBehavior<Exploring>(this, homeAreaToHold());
		}
*/

	if (him().StartingBase())
	{
		if (him().StartingBase()->GetArea()->AccessibleFrom(GetArea()))
		{
			if (!him().StartingBaseDestroyed())
			{
			///	bw << NameWithId() << " go to his base!" << endl; ai()->SetDelay(2000);
				return ChangeBehavior<Raiding>(this, GetRaidingTarget());
			}
			else
			{
				return ChangeBehavior<Exploring>(this, GetArea());
			}
		}
	}
	else
	{
	///	bw << NameWithId() << " go to his base!" << endl; ai()->SetDelay(2000);
		return ChangeBehavior<Scouting>(this);
	}
}

	
} // namespace iron



