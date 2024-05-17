//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "cc.h"
#include "refinery.h"
#include "army.h"
#include "../behavior/defaultBehavior.h"
#include "../behavior/scouting.h"
#include "../behavior/mining.h"
#include "../behavior/refining.h"
#include "../behavior/supplementing.h"
#include "../behavior/exploring.h"
#include "../behavior/harassing.h"
#include "../behavior/executing.h"
#include "../territory/stronghold.h"
#include "../strategy/strategy.h"
#include "../strategy/expand.h"
#include "../strategy/wraithRush.h"
#include "../strategy/terranFastExpand.h"
#include "../strategy/baseDefense.h"
#include "../strategy/walling.h"
#include "../interactive.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Command_Center>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


template<>
class ExpertInTraining<Terran_SCV> : public TrainingExpert
{
public:
						ExpertInTraining(MyBuilding * pWhere) : TrainingExpert(Terran_SCV, pWhere) {}

	void				UpdateTrainingPriority() override;

private:
};




void ExpertInTraining<Terran_SCV>::UpdateTrainingPriority()
{
	const int activeBases = count_if(me().Bases().begin(), me().Bases().end(), [](VBase * b){ return b->Active(); });

	assert_throw(Where()->GetStronghold());
	VBase * base = Where()->GetStronghold()->HasBase();

	if (Where()->TopLeft() != base->BWEMPart()->Location()) { m_priority = 0; return; }

	if ((Units() > 100) || ((Units() > 50) && (me().MineralsAvailable() > 800))) { m_priority = 0; return; }

	int k = Where()->Unit()->isTraining() ? 1 : 0;


	if ((base->LackingMiners() > k) || (base->LackingRefiners() > k) || ((int)base->Supplementers().size() + k == 0))
	{
		if (me().ExceedingSCVsoldiers() - me().SCVsoldiersForever() <= 3)
			m_priority = 600;//me().Army().TerritoryCond() ? 600 : 
		else
			m_priority = 0;
	}
	else
	{
		if (me().LackingSCVsoldiers() > 0)
		{
			m_priority = 380;
		}
		else
		{
			m_priority = 0;
			if (him().IsProtoss() && (activeBases < 2))
				m_priority = 20;
		}
	}

	// SCV are cheap, so we clear the priority if the CC is still training for some time.
	if (m_priority > 0)
		if (me().SupplyAvailable() >= 2)
		{
			double miningCapacity = double(Mining::Instances().size()) / 4 / (1 + Where()->IdlePosition());
			int timeNeeded = int(1.25 * UnitType(Terran_SCV).buildTime() / miningCapacity);
		///	bw << timeNeeded << endl;
			if (Where()->TimeToTrain() > timeNeeded)
				m_priority = 0;
		}

}


template<>
class ExpertInConstructing<Terran_Command_Center> : public ConstructingExpert
{
public:
						ExpertInConstructing() : ConstructingExpert(Terran_Command_Center) {}

	void				UpdateConstructingPriority() override;

private:
};


void ExpertInConstructing<Terran_Command_Center>::UpdateConstructingPriority()
{
	m_priority = 0;

	if (Interactive::expand) { m_priority = 900; return; }

	if (me().MineralsAvailable() > 800)
		{ m_priority = 900; return; }


	if (Builders() < BuildingsUncompleted()) { m_priority = 590; return; }

	if (ai()->GetStrategy()->Active<Expand>())
		if (me().Bases().back()->NeverBuiltYet())
		{/*
			if (him().IsProtoss() && (me().Bases().size() == 2))
			{
				m_priority = ai()->GetStrategy()->Active<BaseDefense>() ? 450 : 650;
				return;
			}*/

			m_priority = 450;

			if (him().IsZerg())
				if (him().Units(Zerg_Mutalisk).empty())
					if (me().Bases().size() <= 2)
					{
						m_priority = 850;
						/*
						if (!him().Buildings(Zerg_Spire).empty())
						{
							if (him().Buildings(Zerg_Spire).front()->Completed())
								m_priority = 450;
						}*/

						//if (him().ZerglingPressure() || him().HydraPressure())
						//	{ m_priority = 350; return; }
					}

			if (auto s = ai()->GetStrategy()->Active<Expand>())
				if (s->LiftCC())
					if (Buildings() < 2)
					{
						m_priority = 595;
						return;
					}
					else
					{
						m_priority = 0;
						return;
					}

//			if (ai()->GetStrategy()->Detected<WraithRush>())
//				m_priority = 850;

			if (ai()->GetStrategy()->Detected<TerranFastExpand>())
				if (me().MineralsAvailable() >= 300)
					m_priority = 650;

//			if (him().MayDarkTemplar())
//				m_priority = 650;

			if (!him().MayDarkTemplar())
			if (!ai()->GetStrategy()->Detected<WraithRush>())
			if (!ai()->GetStrategy()->Detected<TerranFastExpand>())
				if (!me().Army().GoodValueInTime() && (me().MineralsAvailable() <= 250) ||
					ai()->GetStrategy()->Active<BaseDefense>() && (me().Bases().size() <= 2))
					m_priority = 350;

/*
			if (me().Bases().size() <= 2)
				if (him().MayMuta() || (him().Buildings(Zerg_Creep_Colony).size() + him().Buildings(Zerg_Sunken_Colony).size() >= 3))
					m_priority = 300 + 15*me().Buildings(Terran_Missile_Turret).size();
*/

			m_priority += (ai()->Frame() - me().Bases().back()->CreationTime())/20;

			return;
		}

	for (VBase * base : me().Bases())
		if (base->Lost())
			if (base->ShouldRebuild())
			{
				m_priority = 440;
				return;
			}
}


ExpertInConstructing<Terran_Command_Center>	My<Terran_Command_Center>::m_ConstructingExpert;

ConstructingExpert * My<Terran_Command_Center>::GetConstructingExpert() { return &m_ConstructingExpert; }


My<Terran_Command_Center>::My(BWAPI::Unit u)
	: MyBuilding(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Command_Center);

	AddTrainingExpert<Terran_SCV>();

	m_ConstructingExpert.OnBuildingCreated();
}


void My<Terran_Command_Center>::DefaultBehaviorOnFrame()
{CI(this);
	if (DefaultBehaviorOnFrame_common()) return;

	if (Unit()->isTraining()) return;

	const bool underAttack = Life() < PrevLife(60);
	const Base * pMyStartingBase = me().StartingBase();

	static frame_t lastTimeUnderAttack = 0;
	if (underAttack) lastTimeUnderAttack = ai()->Frame();

	if (auto s = ai()->GetStrategy()->Active<Expand>())
		if (s->LiftCC())
		{
			if (Flying())
			{
				bool threat = false;
				for (UnitType t : {Protoss_Dragoon, Protoss_Zealot})
					for (HisUnit * u : him().Units(t))
						if (distToRectangle(u->Pos(), TopLeft(), Size()) < 5*32)
							threat = true;

				if (underAttack && ((Life() < 900) || (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) < 7)) ||
					threat && (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) < 5))
				{
					if (CanAcceptCommand())
						return Move(pMyStartingBase->Center());
				}
				else
					if (ai()->Frame() % 10 == 0)
						if (ai()->Frame() - lastTimeUnderAttack > 50)
							if (!Land(me().Bases().back()->BWEMPart()->Location(), check_t::no_check))
								for (MyUnit * u : me().Units())
									if (u->Completed() && !u->Loaded())
										if (u->CanAcceptCommand())
											if (u->Pos().getApproxDistance(me().Bases().back()->BWEMPart()->Center()) < 3*32)
												u->Move(ai()->GetVMap().RandomPosition(Position(me().Bases().back()->BWEMPart()->GetArea()->Top()), 6*32));
			}
			else
			{
//				if (underAttack)
					if (!ai()->GetStrategy()->Active<Walling>())
						if (TopLeft() != pMyStartingBase->Location())
							if (GetArea() == me().GetArea())
								return Lift(check_t::no_check);

				if (underAttack)
					if (TopLeft() == me().Bases().back()->BWEMPart()->Location())
						return Lift(check_t::no_check);
			}

			return;
		}

	if (double(Life()) / MaxLife() < 0.60)
	{
		if (Flying())
		{
			if (underAttack)
			{
				if (!Unit()->isMoving())
					return Move(ai()->GetVMap().RandomSeaPosition());
			}
			else
				if (ai()->Frame() % 10 == 0)
					if (!Land(pMyStartingBase->Location(), check_t::no_check))
						for (MyUnit * u : me().Units())
							if (u->Completed() && !u->Loaded())
								if (u->CanAcceptCommand())
									if (u->Pos().getApproxDistance(pMyStartingBase->Center()) < 3*32)
										u->Move(ai()->GetVMap().RandomPosition(Position(pMyStartingBase->GetArea()->Top()), 5*32));

			return;
		}
		else
		{
			if (underAttack)
				if (TopLeft() == pMyStartingBase->Location())
					return Lift(check_t::no_check);
		}
	}
}




//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_SCV>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

My<Terran_SCV>::My(BWAPI::Unit u)
	: MyUnit(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_SCV);
}


void My<Terran_SCV>::DefaultBehaviorOnFrame()
{CI(this);
//	return ChangeBehavior<Executing>(this);

//	const int countSCVatHome = count_if(me().Units(Terran_SCV).begin(), me().Units(Terran_SCV).end(),
//		[](const unique_ptr<MyUnit> & u){ return u->GetArea() == me().GetArea(); });

//	if ((GetArea() == base->BWEMPart()->GetArea()) || ((int)Mining::Instances().size() < 4))
	if (GetStronghold())
	{
		auto * base = GetStronghold()->HasBase();

		if (!base->Active())
			return ChangeBehavior<Exploring>(this, base->GetArea()->BWEMPart());

		if (base->LackingMiners() > 0)
//		if ((GetArea() == me().GetArea()) ||						// SCVs at home should mine by default
//			(Mining::Instances().empty() && (countSCVatHome == 0)))		// SCVs not at home are sent to mine only if no miner exist and no SCV is at home (SCV at home will mine soon or later).
			return ChangeBehavior<Mining>(this);

		if (base->LackingRefiners() > 0)
			return ChangeBehavior<Refining>(this);

		if (base->LackingSupplementers() > 0)
		{
			if (me().LackingSCVsoldiers() <= 0)
				return ChangeBehavior<Supplementing>(this);
		}
	}
	else if (!SoldierForever())
		for (const VBase * base : me().Bases())
			if (base->Active())
			{
				int k = base->GetCC()->Unit()->isTraining() ? 1 : 0;
				if ((base->LackingMiners() > k) || (base->LackingRefiners() > k) || ((int)base->Supplementers().size() + k == 0))
					if ((Mining::Instances().size() < 2) || (me().ExceedingSCVsoldiers() - me().SCVsoldiersForever() > 3))
					{
					///	bw << NameWithId() << " go back to CC" << endl;
						EnterStronghold(base->GetStronghold());
						return ChangeBehavior<Supplementing>(this);
					}
			}

	//Move(ai()->GetMap().RandomPosition());
	return ChangeBehavior<Scouting>(this);

//	if (him().Accessible())
//		return ChangeBehavior<Harassing>(this);
}


void My<Terran_SCV>::Gather(Mineral * m, check_t checkMode)
{CI(this);
///	bw << ai()->Frame() << ") " << NameWithId() + " gather Mineral #" << m->Unit()->getID() << endl;
	bool result = Unit()->gather(m->Unit());
	OnCommandSent(checkMode, result, NameWithId() + " gather " + "Mineral #" + to_string(m->Unit()->getID()));
}


void My<Terran_SCV>::Gather(Geyser * g, check_t checkMode)
{CI(this);
	My<Terran_Refinery> * pRefinery = static_cast<My<Terran_Refinery> *>(g->Ext());
	assert_throw(pRefinery);
	bool result = Unit()->gather(pRefinery->Unit());
	OnCommandSent(checkMode, result, NameWithId() + " gather " + "Refinery #" + to_string(pRefinery->Unit()->getID()));
}


void My<Terran_SCV>::ReturnCargo(check_t checkMode)
{CI(this);
	bool result = Unit()->returnCargo();
	OnCommandSent(checkMode, result, NameWithId() + " returnCargo");
}


void My<Terran_SCV>::Repair(MyBWAPIUnit * target, check_t checkMode)
{CI(this);
///	bw << NameWithId() + " repair " + target->NameWithId() << endl;
	bool result = Unit()->repair(target->Unit());
	OnCommandSent(checkMode, result, NameWithId() + " repair " + target->NameWithId());
}


bool My<Terran_SCV>::Build(BWAPI::UnitType type, TilePosition location, check_t checkMode)
{CI(this);
///	bw << NameWithId() + " builds " + type.getName() << endl;
	bool result = Unit()->build(type, location);
	OnCommandSent(checkMode, result, NameWithId() + " build " + type.getName());
	return result;
}


	
} // namespace iron



